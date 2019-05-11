#if defined(_HAVE_FFMPEG_)
#include "ffmpeg-live-source.h"
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

AVCodecContext* ffencoder_create(AVCodecParameters* codecpar)
{
	int ret;
	AVCodec* codec = NULL;
	AVDictionary *opts = NULL;
	AVCodecContext* avctx = NULL;

	codec = avcodec_find_encoder(codecpar->codec_id);
	if (NULL == codec)
	{
		printf("[%s] avcodec_find_encoder(%d) not found.\n", __FUNCTION__, codecpar->codec_id);
		return NULL;
	}

	avctx = avcodec_alloc_context3(codec);
	if (NULL == avctx)
		return NULL;

	ret = avcodec_parameters_to_context(avctx, codecpar);
	if (ret < 0)
	{
		avcodec_free_context(&avctx);
		return NULL;
	}
	avctx->time_base = av_make_q(1, 1000); // 1ms

	ret = avcodec_open2(avctx, codec, &opts);
	av_dict_free(&opts);
	if (ret < 0)
	{
		printf("[%s] avcodec_open2(%d) => %d.\n", __FUNCTION__, codecpar->codec_id, ret);
		avcodec_free_context(&avctx);
		return NULL;
	}

	avcodec_parameters_from_context(codecpar, avctx);
	return avctx;
}

AVCodecContext* ffdecoder_create(AVCodecParameters* codecpar)
{
	int ret;
	AVCodec* codec = NULL;
	AVDictionary *opts = NULL;
	AVCodecContext* avctx = NULL;

	codec = avcodec_find_decoder(codecpar->codec_id);
	if (NULL == codec)
	{
		printf("[%s] avcodec_find_decoder(%d) not found.\n", __FUNCTION__, codecpar->codec_id);
		return NULL;
	}

	avctx = avcodec_alloc_context3(codec);
	if (NULL == avctx)
		return NULL;

	ret = avcodec_parameters_to_context(avctx, codecpar);
	if (ret < 0)
	{
		avcodec_free_context(&avctx);
		return NULL;
	}
	avctx->time_base = av_make_q(1, 1000); // 1ms

	ret = avcodec_open2(avctx, codec, &opts);
	av_dict_free(&opts);
	if (ret < 0)
	{
		printf("[%s] avcodec_open2(%d) => %d.\n", __FUNCTION__, codecpar->codec_id, ret);
		avcodec_free_context(&avctx);
		return NULL;
	}

	avcodec_parameters_from_context(codecpar, avctx);
	return avctx;
}

int ffencoder_input(AVCodecContext *avctx, const AVFrame* frame)
{
	int ret = avcodec_send_frame(avctx, frame);
	if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
	{
		printf("[%s] avcodec_send_frame() => %d\n", __FUNCTION__, ret);
		return ret;
	}
	//if (ret >= 0)
	//	pkt.size = 0;

	return 0;
}

int ffencoder_getpacket(AVCodecContext *avctx, AVPacket* pkt)
{
	int ret = avcodec_receive_packet(avctx, pkt);
	if (ret >= 0)
	{
		// ok
	}

	//if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
	//	ret = 0;

	return ret;
}

FFLiveSource::FFLiveSource(const char *camera)
{
	static int s_init = 0;
	if (0 == s_init)
	{
		s_init = 1;
		av_register_all();
		avformat_network_init();
		avdevice_register_all();
	}

	m_speed = 1.0;
	m_status = 0;
	m_clock = 0;
	m_count = 0;
	av_init_packet(&m_pkt);

	if (0 == Open(camera))
	{
		for (unsigned int i = 0; i < m_ic->nb_streams; i++)
		{
			AVCodecParameters* codecpar = m_ic->streams[i]->codecpar;
			if (AVMEDIA_TYPE_VIDEO == codecpar->codec_type)
			{
				OnVideo(codecpar);
			}
			else if (AVMEDIA_TYPE_AUDIO == codecpar->codec_type)
			{
				OnAudio(codecpar);
			}
		}
	}

	for (int i = 0; i < m_count; i++)
	{
		struct media_t* m = &m_media[i];
		rtp_set_info(m->rtp, "RTSPServer", path_basename(camera));
	}
}

FFLiveSource::~FFLiveSource()
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

		if(m->encoder)
			avcodec_free_context(&m->encoder);
		if (m->decoder)
			avcodec_free_context(&m->decoder);
	}

	if (m_ic)
	{
		avformat_close_input(&m_ic);
		avformat_free_context(m_ic);
	}
}

int FFLiveSource::Open(const char* camera)
{
	int r;
	AVDictionary* opt = NULL;
	m_ic = avformat_alloc_context();
	if (NULL == m_ic)
	{
		printf("%s(%s): avformat_alloc_context failed.\n", __FUNCTION__, camera);
		return ENOMEM;
	}

	//if (!av_dict_get(ff->opt, "scan_all_pmts", NULL, AV_DICT_MATCH_CASE)) {
	//	av_dict_set(&ff->opt, "scan_all_pmts", "1", AV_DICT_DONT_OVERWRITE);
	//	scan_all_pmts_set = 1;
	//}

	AVInputFormat *ifmt = av_find_input_format("dshow");
	r = avformat_open_input(&m_ic, camera, ifmt, NULL/*&opt*/);
	if (0 != r)
	{
		printf("%s: avformat_open_input(%s) => %d\n", __FUNCTION__, camera, r);
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
		printf("%s(%s): could not find codec parameters\n", __FUNCTION__, camera);
		return r;
	}

	av_dict_free(&opt);
	return 0;
}

int FFLiveSource::SetTransport(const char* track, std::shared_ptr<IRTPTransport> transport)
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

int FFLiveSource::Play()
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
			if (0 == strcmp("H264", m->name))
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

int FFLiveSource::Pause()
{
	m_status = 2;
	m_clock = 0;
	m_dts = -1;
	return 0;
}

int FFLiveSource::Seek(int64_t pos)
{
	return -1;
}

int FFLiveSource::SetSpeed(double speed)
{
	return -1;
}

int FFLiveSource::GetDuration(int64_t& duration) const
{
	return -1;
}

int FFLiveSource::GetSDPMedia(std::string& sdp) const
{
	sdp = m_sdp;
	return m_ic ? 0 : -1;
}

int FFLiveSource::GetRTPInfo(const char* uri, char *rtpinfo, size_t bytes) const
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

void FFLiveSource::OnVideo(AVCodecParameters* codecpar)
{
	int n = 0;
	uint8_t buffer[8 * 1024];
	AVCodecParameters* par;
	struct media_t* m = &m_media[m_count++];

	m->decoder = ffdecoder_create(codecpar);
	par = avcodec_parameters_alloc();
	avcodec_parameters_copy(par, codecpar);
	par->codec_id = AV_CODEC_ID_H264;
	m->encoder = ffencoder_create(par);
	avcodec_parameters_free(&par);

	m->track = m_count-1;
	m->rtcp_clock = 0;
	m->ssrc = rtp_ssrc();
	m->timestamp = rtp_ssrc();
	m->bandwidth = 4 * 1024 * 1024;
	m->dts_last = m->dts_first = -1;

	if (AV_CODEC_ID_H264 == par->codec_id)
	{
		struct mpeg4_avc_t avc;
		mpeg4_avc_decoder_configuration_record_load(m->encoder->extradata, m->encoder->extradata_size, &avc);
		assert(avc.nb_pps + avc.nb_sps > 0);

		static const char* pattern =
			"m=video 0 RTP/AVP %d\n"
			"a=rtpmap:%d H264/90000\n"
			"a=fmtp:%d profile-level-id=%02X%02X%02X;packetization-mode=1;sprop-parameter-sets=";

		n = snprintf((char*)buffer, sizeof(buffer), pattern,
			RTP_PAYLOAD_H264, RTP_PAYLOAD_H264, RTP_PAYLOAD_H264,
			(unsigned int)avc.profile, (unsigned int)avc.compatibility, (unsigned int)avc.level);

		for (uint8_t i = 0; i < avc.nb_sps; i++)
		{
			if (i > 0) buffer[n++] = ',';
			n += base64_encode((char*)buffer + n, avc.sps[i].data, avc.sps[i].bytes);
			buffer[n] = '\0';
		}

		for (uint8_t i = 0; i < avc.nb_pps; i++)
		{
			buffer[n++] = ',';
			n += base64_encode((char*)buffer + n, avc.pps[i].data, avc.pps[i].bytes);
			buffer[n] = '\0';
		}

		buffer[n++] = '\n';
		m->frequency = 90000;
		m->payload = RTP_PAYLOAD_H264;
		snprintf(m->name, sizeof(m->name), "%s", "H264");
	}
	else
	{
		assert(0);
		return;
	}

	struct rtp_payload_t rtpfunc = {
		FFLiveSource::RTPAlloc,
		FFLiveSource::RTPFree,
		FFLiveSource::RTPPacket,
	};
	m->packer = rtp_payload_encode_create(m->payload, m->name, (uint16_t)m->timestamp, m->ssrc, &rtpfunc, m);

	struct rtp_event_t event;
	event.on_rtcp = OnRTCPEvent;
	m->rtp = rtp_create(&event, this, m->ssrc, m->timestamp, m->frequency, m->bandwidth, 1);

	n += snprintf((char*)buffer + n, sizeof(buffer) - n, "a=control:track%d\n", m->track);
	m_sdp += (const char*)buffer;
}

void FFLiveSource::OnAudio(AVCodecParameters* codecpar)
{
	int n = 0;
	uint8_t buffer[2 * 1024];
	AVCodecParameters* par;
	struct media_t* m = &m_media[m_count++];

	m->decoder = ffdecoder_create(codecpar);
	par = avcodec_parameters_alloc();
	avcodec_parameters_copy(par, codecpar);
	par->codec_id = AV_CODEC_ID_H264;
	m->encoder = ffencoder_create(par);
	avcodec_parameters_free(&par);

	m->track = m_count-1;
	m->rtcp_clock = 0;
	m->ssrc = rtp_ssrc();
	m->timestamp = rtp_ssrc();
	m->bandwidth = 128 * 1024;
	m->dts_last = m->dts_first = -1;

	if (AV_CODEC_ID_AAC == par->codec_id)
	{
		struct mpeg4_aac_t aac;
		//aac.profile = MPEG4_AAC_LC;
		//aac.channel_configuration = (uint8_t)channel_count;
		//aac.sampling_frequency_index = (uint8_t)mpeg4_aac_audio_frequency_from(sample_rate);
		mpeg4_aac_audio_specific_config_load(m->encoder->extradata, m->encoder->extradata_size, &aac);
		//assert(aac.sampling_frequency_index == (uint8_t)mpeg4_aac_audio_frequency_from(sample_rate));
		//assert(aac.channel_configuration == channel_count);

		if (0)
		{
			// RFC 6416
			// In the presence of SBR, the sampling rates for the core encoder/
			// decoder and the SBR tool are different in most cases. Therefore,
			// this parameter SHALL NOT be considered as the definitive sampling rate.
			static const char* pattern =
				"m=audio 0 RTP/AVP %d\n"
				"a=rtpmap:%d MP4A-LATM/%d/%d\n"
				"a=fmtp:%d profile-level-id=%d;object=%d;cpresent=0;config=";

			n = snprintf((char*)buffer, sizeof(buffer), pattern,
				RTP_PAYLOAD_MP4A, RTP_PAYLOAD_MP4A, 90000, m->encoder->channels,
				RTP_PAYLOAD_MP4A, mpeg4_aac_profile_level(&aac), aac.profile);

			uint8_t config[6];
			int r = mpeg4_aac_stream_mux_config_save(&aac, config, sizeof(config));
			static const char* hex = "0123456789abcdef";
			for (int i = 0; i < r; i++)
			{
				buffer[n++] = hex[config[i] >> 4];
				buffer[n++] = hex[config[i] & 0x0F];
			}
			buffer[n] = '\0';

			snprintf(m->name, sizeof(m->name), "%s", "MP4A-LATM");
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
			static const char* pattern =
				"m=audio 0 RTP/AVP %d\n"
				"a=rtpmap:%d MPEG4-GENERIC/%d/%d\n"
				"a=fmtp:%d streamType=5;profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=";

			n = snprintf((char*)buffer, sizeof(buffer), pattern,
				RTP_PAYLOAD_MP4A, RTP_PAYLOAD_MP4A, m->encoder->sample_rate, m->encoder->channels, RTP_PAYLOAD_MP4A);

			// For MPEG-4 Audio streams, config is the audio object type specific
			// decoder configuration data AudioSpecificConfig()
			n += base64_encode((char*)buffer + n, m->encoder->extradata, m->encoder->extradata_size);
			buffer[n] = '\0';

			snprintf(m->name, sizeof(m->name), "%s", "MPEG4-GENERIC");
		}

		m->frequency = m->encoder->sample_rate;
		m->payload = RTP_PAYLOAD_MP4A;
		buffer[n++] = '\n';
	}
	else
	{
		assert(0);
		return;
	}

	struct rtp_payload_t rtpfunc = {
		FFLiveSource::RTPAlloc,
		FFLiveSource::RTPFree,
		FFLiveSource::RTPPacket,
	};
	m->packer = rtp_payload_encode_create(m->payload, m->name, (uint16_t)m->timestamp, m->ssrc, &rtpfunc, m);

	struct rtp_event_t event;
	event.on_rtcp = OnRTCPEvent;
	m->rtp = rtp_create(&event, this, m->ssrc, m->timestamp, m->frequency, m->bandwidth, 1);

	n += snprintf((char*)buffer + n, sizeof(buffer) - n, "a=control:track%d\n", m->track);
	m_sdp += (const char*)buffer;
}

void FFLiveSource::OnRTCPEvent(const struct rtcp_msg_t* msg)
{
	msg;
}

void FFLiveSource::OnRTCPEvent(void* param, const struct rtcp_msg_t* msg)
{
	FFLiveSource *self = (FFLiveSource *)param;
	self->OnRTCPEvent(msg);
}

int FFLiveSource::SendBye()
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

int FFLiveSource::SendRTCP(struct media_t* m, uint64_t clock)
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

void* FFLiveSource::RTPAlloc(void* param, int bytes)
{
	struct media_t* m = (struct media_t*)param;
	assert(bytes <= sizeof(m->packet));
	return m->packet;
}

void FFLiveSource::RTPFree(void* param, void *packet)
{
	struct media_t* m = (struct media_t*)param;
	assert(m->packet == packet);
}

void FFLiveSource::RTPPacket(void* param, const void *packet, int bytes, uint32_t /*timestamp*/, int /*flags*/)
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
