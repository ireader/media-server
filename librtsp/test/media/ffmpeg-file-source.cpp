#if defined(_HAVE_FFMPEG_)
#include "ffmpeg-file-source.h"
#include "rtp-profile.h"
#include "rtp-payload.h"
#include "mov-format.h"
#include "mpeg4-avc.h"
#include "mpeg4-aac.h"
#include "sys/system.h"
#include "sys/path.h"
#include "base64.h"
#include "rtp.h"
#include <assert.h>

extern "C" uint32_t rtp_ssrc(void);
extern "C" const struct mov_buffer_t* mov_file_buffer(void);
extern "C" int sdp_h264(uint8_t *data, int bytes, unsigned short port, int payload, int frequence, const void* extra, int extra_size);
extern "C" int sdp_h265(uint8_t *data, int bytes, unsigned short port, int payload, int frequence, const void* extra, int extra_size);
extern "C" int sdp_mpeg4_es(uint8_t *data, int bytes, unsigned short port, int payload, int frequence, const void* extra, int extra_size);
extern "C" int sdp_aac_latm(uint8_t *data, int bytes, unsigned short port, int payload, int sample_rate, int channel_count, const void* extra, int extra_size);
extern "C" int sdp_aac_generic(uint8_t *data, int bytes, unsigned short port, int payload, int sample_rate, int channel_count, const void* extra, int extra_size);
extern "C" int sdp_opus(uint8_t *data, int bytes, unsigned short port, int payload, int sample_rate, int channel_count, const void* extra, int extra_size);
extern "C" int sdp_g711u(uint8_t *data, int bytes, unsigned short port);
extern "C" int sdp_g711a(uint8_t *data, int bytes, unsigned short port);

inline uint8_t ffmpeg_codec_id_2_mp4_object(AVCodecID codecid)
{
	switch (codecid)
	{
	case AV_CODEC_ID_MPEG4:
		return MOV_OBJECT_MP4V;
	case AV_CODEC_ID_H264:
		return MOV_OBJECT_H264;
	case AV_CODEC_ID_HEVC:
		return MOV_OBJECT_HEVC;
	case AV_CODEC_ID_AAC:
		return MOV_OBJECT_AAC;
	case AV_CODEC_ID_OPUS:
		return MOV_OBJECT_OPUS;
	default:
		return 0;
	}
}

FFFileSource::FFFileSource(const char *file)
{
	static int s_init = 0;
	if(0 == s_init)
	{
		s_init = 1;
		av_register_all();
		avformat_network_init();
	}

	m_speed = 1.0;
	m_status = 0;
	m_clock = 0;
	m_count = 0;
	av_init_packet(&m_pkt);

	if (0 == Open(file))
	{
		for (unsigned int i = 0; i < m_ic->nb_streams; i++)
		{
			AVCodecParameters* codecpar = m_ic->streams[i]->codecpar;
			uint8_t object = ffmpeg_codec_id_2_mp4_object(codecpar->codec_id);
			if (0 == object)
			{
//				assert(0);
				continue;
			}
			if (AVMEDIA_TYPE_VIDEO == codecpar->codec_type)
			{
				MP4OnVideo(this, i, object, codecpar->width, codecpar->height, codecpar->extradata, codecpar->extradata_size);
			}
			else if (AVMEDIA_TYPE_AUDIO == codecpar->codec_type)
			{
				MP4OnAudio(this, i, object, codecpar->channels, codecpar->bits_per_raw_sample, codecpar->sample_rate, codecpar->extradata, codecpar->extradata_size);
			}
		}
	}

	for (int i = 0; i < m_count; i++)
	{
		struct media_t* m = &m_media[i];
		rtp_set_info(m->rtp, "RTSPServer", path_basename(file));
	}
}

FFFileSource::~FFFileSource()
{
	for (int i = 0; i < m_count; i++)
	{
		struct media_t* m = &m_media[i];
		if (m->rtp)
		{
			rtp_destroy(m->rtp);
			m->rtp = NULL;
		}

		if (m->packer)
		{
			rtp_payload_encode_destroy(m->packer);
			m->packer = NULL;
		}
	}

	if (m_ic)
	{
		avformat_close_input(&m_ic);
		avformat_free_context(m_ic);
	}
}

int FFFileSource::Open(const char* file)
{
	int r;
	AVDictionary* opt = NULL;
	m_ic = avformat_alloc_context();
	if (NULL == m_ic)
	{
		printf("%s(%s): avformat_alloc_context failed.\n", __FUNCTION__, file);
		return ENOMEM;
	}

	//if (!av_dict_get(ff->opt, "scan_all_pmts", NULL, AV_DICT_MATCH_CASE)) {
	//	av_dict_set(&ff->opt, "scan_all_pmts", "1", AV_DICT_DONT_OVERWRITE);
	//	scan_all_pmts_set = 1;
	//}

	r = avformat_open_input(&m_ic, file, NULL, NULL/*&opt*/);
	if (0 != r)
	{
		printf("%s: avformat_open_input(%s) => %d\n", __FUNCTION__, file, r);
		return r;
	}

	//if (scan_all_pmts_set)
	//	av_dict_set(&format_opts, "scan_all_pmts", NULL, AV_DICT_MATCH_CASE);

	//ff->ic->probesize = 100 * 1024;
	//ff->ic->max_analyze_duration = 5 * AV_TIME_BASE;

	/* If not enough info to get the stream parameters, we decode the
	first frames to get it. (used in mpeg case for example) */
	r = avformat_find_stream_info(m_ic, NULL/*&opt*/);
	if (r < 0) {
		printf("%s(%s): could not find codec parameters\n", __FUNCTION__, file);
		return r;
	}

	av_dict_free(&opt);
	return 0;
}

int FFFileSource::SetTransport(const char* track, std::shared_ptr<IRTPTransport> transport)
{
	int t = atoi(track + 5/*track*/);
	for (int i = 0; i < m_count; i++)
	{
		struct media_t* m = &m_media[i];
		if (t != m->track)
			continue;

		m->transport = transport;
		return 0;
	}
	return -1;
}

int FFFileSource::Play()
{
	bool sendframe = false;
	if (3 == m_status)
		return 0;

SEND_PACKET:
	if (0 == m_pkt.buf)
	{
		int r = av_read_frame(m_ic, &m_pkt);
		if (r == AVERROR_EOF)
		{
			// 0-EOF
			m_status = 3;
			SendBye();
			return 0;
		}
		else if (r < 0)
		{
			// error
			return r;
		}

		for (r = 0; r < m_count; r++)
		{
			struct media_t* m = &m_media[r];
			if (m->track == m_pkt.stream_index)
				break;
		}
		if (r == m_count)
		{
			av_packet_unref(&m_pkt); // send flag
			sendframe = 1;
			goto SEND_PACKET;
		}

		AVRational time_base = { 1, 1000/*ms*/ };
		m_pkt.dts = (AV_NOPTS_VALUE == m_pkt.dts ? m_pkt.pts : m_pkt.dts);
		m_pkt.pts = (AV_NOPTS_VALUE == m_pkt.pts ? m_pkt.dts : m_pkt.pts);
		m_pkt.dts = av_rescale_q(m_pkt.dts, m_ic->streams[m_pkt.stream_index]->time_base, time_base);
		m_pkt.pts = av_rescale_q(m_pkt.pts, m_ic->streams[m_pkt.stream_index]->time_base, time_base);
	}

	m_status = 1;
	uint64_t clock = system_clock();
	for (int i = 0; i < m_count; i++)
	{
		struct media_t* m = &m_media[i];
		if (m->track != m_pkt.stream_index)
			continue;

		if (0 == m_clock || m_clock > clock)
			m_clock = clock;
		if (-1 == m_dts)
			m_dts = m_pkt.dts;

		if (int64_t(clock - m_clock) + m_dts >= m_pkt.dts)
		{
			if (0 == strcmp("H264", m->name) || 0 == strcmp("H265", m->name))
			{
				// MPEG4 -> H.264 byte stream
				uint8_t* p = m_pkt.data;
				size_t bytes = m_pkt.size;
				while (bytes > 0)
				{
					// nalu size -> start code
					assert(bytes > 4);
					uint32_t n = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
					p[0] = 0;
					p[1] = 0;
					p[2] = 0;
					p[3] = 1;
					bytes -= n + 4;
					p += n + 4;
				}

				//printf("[V] pts: %lld, dts: %lld, clock: %llu\n", m_pkt.pts, m_pkt.dts, clock);
			}
			else if (0 == strcmp("MP4A-LATM", m->name) || 0 == strcmp("MPEG4-GENERIC", m->name))
			{
				// add ADTS header
				//printf("[A] pts: %lld, dts: %lld, clock: %llu\n", m_pkt.pts, m_pkt.dts, clock);
			}
			else
			{
				assert(0);
			}

			if (-1 == m->dts_first)
				m->dts_first = m_pkt.pts;
			m->dts_last = m_pkt.pts;
			uint32_t timestamp = m->timestamp + m->dts_last - m->dts_first;

			rtp_payload_encode_input(m->packer, m_pkt.data, m_pkt.size, (uint32_t)(timestamp * (m->frequency / 1000) /*kHz*/));

			av_packet_unref(&m_pkt); // send flag
			sendframe = 1;
			goto SEND_PACKET;
		}

		break;
	}

	return sendframe ? 1 : 0;
}

int FFFileSource::Pause()
{
	m_status = 2;
	m_clock = 0;
	m_dts = -1;
	return 0;
}

int FFFileSource::Seek(int64_t pos)
{
	// update timestamp
	for (int i = 0; i < m_count; i++)
	{
		if (-1 != m_media[i].dts_first)
			m_media[i].timestamp += m_media[i].dts_last - m_media[i].dts_first + 1;
		m_media[i].dts_first = -1;
		//SendRTCP(&m_media[i], system_clock());
	}

	m_dts = -1;
	m_clock = 0;
	av_packet_unref(&m_pkt); // clear buffered frame

	AVRational time_base = { 1, 1000/*ms*/ };
	pos = av_rescale_q(pos, time_base, m_ic->streams[0]->time_base);
	return av_seek_frame(m_ic, 0, pos, 0);
}

int FFFileSource::SetSpeed(double speed)
{
	m_speed = speed;
	return 0;
}

int FFFileSource::GetDuration(int64_t& duration) const
{
	if (m_ic)
	{
		duration = m_ic->duration / 1000;
		return 0;
	}
	return -1;
}

int FFFileSource::GetSDPMedia(std::string& sdp) const
{
	sdp = m_sdp;
	return m_ic ? 0 : -1;
}

int FFFileSource::GetRTPInfo(const char* uri, char *rtpinfo, size_t bytes) const
{
	int n = 0;
	uint16_t seq;
	uint32_t timestamp;

	// RTP-Info: url=rtsp://foo.com/bar.avi/streamid=0;seq=45102,
	//			 url=rtsp://foo.com/bar.avi/streamid=1;seq=30211
	for (int i = 0; i < m_count; i++)
	{
		const struct media_t* m = &m_media[i];
		rtp_payload_encode_getinfo(m->packer, &seq, &timestamp);

		if (i > 0)
			rtpinfo[n++] = ',';
		n += snprintf(rtpinfo + n, bytes - n, "url=%s/track%d;seq=%hu;rtptime=%u", uri, m->track, seq, (unsigned int)(m->timestamp * (m->frequency / 1000) /*kHz*/));
	}
	return 0;
}

void FFFileSource::MP4OnVideo(void* param, uint32_t track, uint8_t object, int /*width*/, int /*height*/, const void* extra, size_t bytes)
{
	int n = 0;
	uint8_t buffer[8 * 1024];
	FFFileSource* self = (FFFileSource*)param;
	struct media_t* m = &self->m_media[self->m_count++];
	m->track = track;
	m->rtcp_clock = 0;
	m->ssrc = rtp_ssrc();
	m->timestamp = rtp_ssrc();
	m->bandwidth = 4 * 1024 * 1024;
	m->dts_last = m->dts_first = -1;

	if (MOV_OBJECT_H264 == object)
	{
		m->frequency = 90000;
		m->payload = RTP_PAYLOAD_H264;
		snprintf(m->name, sizeof(m->name), "%s", "H264");
		n = sdp_h264(buffer, sizeof(buffer), 0, RTP_PAYLOAD_H264, 90000, extra, bytes);
	}
	else if (MOV_OBJECT_HEVC == object)
	{
		m->frequency = 90000;
		m->payload = RTP_PAYLOAD_H265;
		snprintf(m->name, sizeof(m->name), "%s", "H265");
		n = sdp_h265(buffer, sizeof(buffer), 0, RTP_PAYLOAD_H265, 90000, extra, bytes);
	}
	else
	{
		assert(0);
		return;
	}

	struct rtp_payload_t rtpfunc = {
		FFFileSource::RTPAlloc,
		FFFileSource::RTPFree,
		FFFileSource::RTPPacket,
	};
	m->packer = rtp_payload_encode_create(m->payload, m->name, (uint16_t)m->timestamp, m->ssrc, &rtpfunc, m);

	struct rtp_event_t event;
	event.on_rtcp = OnRTCPEvent;
	m->rtp = rtp_create(&event, self, m->ssrc, m->timestamp, m->frequency, m->bandwidth, 1);

	n += snprintf((char*)buffer + n, sizeof(buffer) - n, "a=control:track%d\n", m->track);
	self->m_sdp += (const char*)buffer;
}

void FFFileSource::MP4OnAudio(void* param, uint32_t track, uint8_t object, int channel_count, int /*bit_per_sample*/, int sample_rate, const void* extra, size_t bytes)
{
	int n = 0;
	uint8_t buffer[2 * 1024];
	FFFileSource* self = (FFFileSource*)param;
	struct media_t* m = &self->m_media[self->m_count++];
	m->track = track;
	m->rtcp_clock = 0;
	m->ssrc = rtp_ssrc();
	m->timestamp = rtp_ssrc();
	m->bandwidth = 128 * 1024;
	m->dts_last = m->dts_first = -1;

	if (MOV_OBJECT_AAC == object)
	{
		struct mpeg4_aac_t aac;
		//aac.profile = MPEG4_AAC_LC;
		//aac.channel_configuration = (uint8_t)channel_count;
		//aac.sampling_frequency_index = (uint8_t)mpeg4_aac_audio_frequency_from(sample_rate);
		mpeg4_aac_audio_specific_config_load((const uint8_t*)extra, bytes, &aac);
		//assert(aac.sampling_frequency_index == (uint8_t)mpeg4_aac_audio_frequency_from(sample_rate));
		//assert(aac.channel_configuration == channel_count);

		if (0)
		{
			// RFC 6416
			// In the presence of SBR, the sampling rates for the core encoder/
			// decoder and the SBR tool are different in most cases. Therefore,
			// this parameter SHALL NOT be considered as the definitive sampling rate.
			m->frequency = sample_rate;
			m->payload = RTP_PAYLOAD_MP4A;
			snprintf(m->name, sizeof(m->name), "%s", "MP4A-LATM");
			n = sdp_aac_latm(buffer, sizeof(buffer), 0, RTP_PAYLOAD_MP4A, sample_rate, channel_count, extra, bytes);
		}
		else
		{
			// RFC 3640 3.3.1. General (p21)
			// a=rtpmap:<payload type> <encoding name>/<clock rate>[/<encoding parameters > ]
			// For audio streams, <encoding parameters> specifies the number of audio channels
			// streamType: AudioStream
			// When using SDP, the clock rate of the RTP time stamp MUST be expressed using the "rtpmap" attribute. 
			// If an MPEG-4 audio stream is transported, the rate SHOULD be set to the same value as the sampling rate of the audio stream. 
			// If an MPEG-4 video stream transported, it is RECOMMENDED that the rate be set to 90 kHz.
			m->frequency = sample_rate;
			m->payload = RTP_PAYLOAD_MP4A;
			snprintf(m->name, sizeof(m->name), "%s", "MPEG4-GENERIC");
			n = sdp_aac_generic(buffer, sizeof(buffer), 0, RTP_PAYLOAD_MP4A, sample_rate, channel_count, extra, bytes);
		}
	}
	else if (MOV_OBJECT_OPUS == object)
	{
		// RFC7587 RTP Payload Format for the Opus Speech and Audio Codec
		m->frequency = sample_rate;
		m->payload = RTP_PAYLOAD_OPUS;
		snprintf(m->name, sizeof(m->name), "%s", "opus");
		n = sdp_opus(buffer, sizeof(buffer), 0, RTP_PAYLOAD_OPUS, sample_rate, channel_count, extra, bytes);
	}
	else if (MOV_OBJECT_G711u == object)
	{
		m->frequency = sample_rate;
		m->payload = RTP_PAYLOAD_PCMU;
		snprintf(m->name, sizeof(m->name), "%s", "PCMU");
		n = sdp_g711u(buffer, sizeof(buffer), 0);
	}
	else
	{
		assert(0);
		return;
	}

	struct rtp_payload_t rtpfunc = {
		FFFileSource::RTPAlloc,
		FFFileSource::RTPFree,
		FFFileSource::RTPPacket,
	};
	m->packer = rtp_payload_encode_create(m->payload, m->name, (uint16_t)m->timestamp, m->ssrc, &rtpfunc, m);

	struct rtp_event_t event;
	event.on_rtcp = OnRTCPEvent;
	m->rtp = rtp_create(&event, self, m->ssrc, m->timestamp, m->frequency, m->bandwidth, 1);

	n += snprintf((char*)buffer + n, sizeof(buffer) - n, "a=control:track%d\n", m->track);
	self->m_sdp += (const char*)buffer;
}

void FFFileSource::OnRTCPEvent(const struct rtcp_msg_t* msg)
{
	msg;
}

void FFFileSource::OnRTCPEvent(void* param, const struct rtcp_msg_t* msg)
{
	FFFileSource *self = (FFFileSource *)param;
	self->OnRTCPEvent(msg);
}

int FFFileSource::SendBye()
{
	char rtcp[1024] = { 0 };
	for (int i = 0; i < m_count; i++)
	{
		struct media_t* m = &m_media[i];

		size_t n = rtp_rtcp_bye(m->rtp, rtcp, sizeof(rtcp));

		// send RTCP packet
		m->transport->Send(true, rtcp, n);
	}

	return 0;
}

int FFFileSource::SendRTCP(struct media_t* m, uint64_t clock)
{
	// make sure have sent RTP packet

	int interval = rtp_rtcp_interval(m->rtp);
	if (0 == m->rtcp_clock || m->rtcp_clock + interval < clock)
	{
		char rtcp[1024] = { 0 };
		size_t n = rtp_rtcp_report(m->rtp, rtcp, sizeof(rtcp));

		// send RTCP packet
		m->transport->Send(true, rtcp, n);

		m->rtcp_clock = clock;
	}

	return 0;
}

void* FFFileSource::RTPAlloc(void* param, int bytes)
{
	struct media_t* m = (struct media_t*)param;
	assert(bytes <= sizeof(m->packet));
	return m->packet;
}

void FFFileSource::RTPFree(void* param, void *packet)
{
	struct media_t* m = (struct media_t*)param;
	assert(m->packet == packet);
}

void FFFileSource::RTPPacket(void* param, const void *packet, int bytes, uint32_t /*timestamp*/, int /*flags*/)
{
	struct media_t* m = (struct media_t*)param;
	assert(m->packet == packet);

	// Hack: Send an initial RTCP "SR" packet, before the initial RTP packet, 
	// so that receivers will (likely) be able to get RTCP-synchronized presentation times immediately:
	rtp_onsend(m->rtp, packet, bytes/*, time*/);
	SendRTCP(m, system_clock());
	
	int r = m->transport->Send(false, packet, bytes);
	assert(r == (int)bytes);
}

#endif
