#include "mp4-file-source.h"
#include "mov-format.h"
#include "rtp.h"
#include "base64.h"
#include "mpeg4-avc.h"
#include "mpeg4-aac.h"
#include "rtp-profile.h"
#include "rtp-payload.h"
#include "sys/system.h"
#include "sys/path.h"
#include <assert.h>

extern "C" int rtp_ssrc(void);
extern "C" const struct mov_buffer_t* mov_file_buffer(void);

MP4FileSource::MP4FileSource(const char *file)
{
	m_speed = 1.0;
	m_status = 0;
	m_clock = 0;
	m_frame.bytes = 0;
	m_count = 0;

	m_fp = fopen(file, "rb");
	m_reader = mov_reader_create(mov_file_buffer(), m_fp);
	if (m_reader)
	{
		struct mov_reader_trackinfo_t info = { MP4OnVideo, MP4OnAudio };
		mov_reader_getinfo(m_reader, &info, this);
	}

	for (int i = 0; i < m_count; i++)
	{
		struct media_t* m = &m_media[i];
		rtp_set_info(m->rtp, "RTSPServer", path_basename(file));
	}
}

MP4FileSource::~MP4FileSource()
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

	if (m_reader)
	{
		mov_reader_destroy(m_reader);
		m_reader = NULL;
	}
	if (m_fp)
		fclose(m_fp);
}

int MP4FileSource::SetTransport(const char* track, std::shared_ptr<IRTPTransport> transport)
{
	int t = atoi(track + 5/*track*/);
	for (int i = 0; i < m_count; i++)
	{
		struct media_t* m = &m_media[i];
		if(t != m->track)
			continue;

		m->transport = transport;
		return 0;
	}
	return -1;
}

int MP4FileSource::Play()
{
	bool sendframe = false;
	if (3 == m_status)
		return 0;

SEND_PACKET:
	if (0 == m_frame.bytes)
	{
		int r = mov_reader_read(m_reader, m_frame.buffer, sizeof(m_frame.buffer), MP4OnRead, &m_frame);
		if (r == 0)
		{
			// 0-EOF
			m_status = 3;
			SendBye();
			return r;
		}
		else if (r < 0)
		{
			// error
			return r;
		}
	}

	m_status = 1;
	uint64_t clock = system_clock();
	for (int i = 0; i < m_count; i++)
	{
		struct media_t* m = &m_media[i];
		if (m->track != m_frame.track)
			continue;

		if (0 == m_clock || m_clock > clock)
			m_clock = clock;
		if (-1 == m_dts)
			m_dts = m_frame.dts;

		if (int64_t(clock - m_clock) + m_dts >= m_frame.dts)
		{
			if (0 == strcmp("H264", m->name))
			{
				// MPEG4 -> H.264 byte stream
				uint8_t* p = m_frame.buffer;
				size_t bytes = m_frame.bytes;
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

				//printf("[V] pts: %lld, dts: %lld, clock: %llu\n", m_frame.pts, m_frame.dts, clock);
			}
			else if (0 == strcmp("MP4A-LATM", m->name) || 0 == strcmp("MPEG4-GENERIC", m->name))
			{
				// add ADTS header
				//printf("[A] pts: %lld, dts: %lld, clock: %llu\n", m_frame.pts, m_frame.dts, clock);
			}
			else
			{
				assert(0);
			}

			if (-1 == m->dts_first)
				m->dts_first = m_frame.pts;
			m->dts_last = m_frame.pts;
			uint32_t timestamp = m->timestamp + m->dts_last - m->dts_first;
/*
			if (-1 == m->dts)
				m->dts = m_frame.dts;
			m->timestamp += m_frame.dts - m->dts;
			m->dts = m_frame.dts;
*/
			rtp_payload_encode_input(m->packer, m_frame.buffer, m_frame.bytes, (uint32_t)(timestamp * (m->frequency / 1000) /*kHz*/));
			SendRTCP(m, clock);

			m_frame.bytes = 0; // send flag
			sendframe = 1;
			goto SEND_PACKET;
		}

		break;
	}

	return sendframe ? 1 : 0;
}

int MP4FileSource::Pause()
{
	m_status = 2;
	m_clock = 0;
	m_dts = -1;
	return 0;
}

int MP4FileSource::Seek(int64_t pos)
{
	// update timestamp
	for (int i = 0; i < m_count; i++)
	{
		//m_media[i].dts = -1;
		//m_media[i].timestamp += 1;
		if (-1 != m_media[i].dts_first)
			m_media[i].timestamp += m_media[i].dts_last - m_media[i].dts_first + 1;
		m_media[i].dts_first = -1;
	}

	m_dts = pos;
	m_clock = 0;
	m_frame.bytes = 0; // clear buffered frame
	return mov_reader_seek(m_reader, &m_dts);
}

int MP4FileSource::SetSpeed(double speed)
{
	m_speed = speed;
	return 0;
}

int MP4FileSource::GetDuration(int64_t& duration) const
{
	if (m_reader)
	{
		duration = (int64_t)mov_reader_getduration(m_reader);
		return 0;
	}
	return -1;
}

int MP4FileSource::GetSDPMedia(std::string& sdp) const
{
	sdp = m_sdp;
	return m_reader ? 0 : -1;
}

int MP4FileSource::GetRTPInfo(const char* uri, char *rtpinfo, size_t bytes) const
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

void MP4FileSource::MP4OnVideo(void* param, uint32_t track, uint8_t object, int /*width*/, int /*height*/, const void* extra, size_t bytes)
{
	int n = 0;
	MP4FileSource* self = (MP4FileSource*)param;
	struct media_t* m = &self->m_media[self->m_count++];
	m->track = track;
	m->rtcp_clock = 0;
	m->ssrc = (uint32_t)rtp_ssrc();
	m->timestamp = m->ssrc;
	m->bandwidth = 4 * 1024 * 1024;
	m->dts_first = -1;
	m->dts_last = -1;

	if (MOV_OBJECT_H264 == object)
	{
		struct mpeg4_avc_t avc;
		mpeg4_avc_decoder_configuration_record_load((const uint8_t*)extra, bytes, &avc);
		assert(avc.nb_pps + avc.nb_sps > 0);

		static const char* pattern =
			"m=video 0 RTP/AVP %d\n"
			"a=rtpmap:%d H264/90000\n"
			"a=fmtp:%d profile-level-id=%02X%02X%02X;packetization-mode=1;sprop-parameter-sets=";

		n = snprintf((char*)self->m_frame.buffer, sizeof(self->m_frame.buffer), pattern,
			RTP_PAYLOAD_H264, RTP_PAYLOAD_H264, RTP_PAYLOAD_H264,
			(unsigned int)avc.profile, (unsigned int)avc.compatibility, (unsigned int)avc.level);

		for (uint8_t i = 0; i < avc.nb_sps; i++)
		{
			if(i > 0) self->m_frame.buffer[n++] = ',';
			n += base64_encode((char*)self->m_frame.buffer + n, avc.sps[i].data, avc.sps[i].bytes);
			self->m_frame.buffer[n] = '\0';
		}

		for (uint8_t i = 0; i < avc.nb_pps; i++)
		{
			self->m_frame.buffer[n++] = ',';
			n += base64_encode((char*)self->m_frame.buffer + n, avc.pps[i].data, avc.pps[i].bytes);
			self->m_frame.buffer[n] = '\0';
		}

		self->m_frame.buffer[n++] = '\n';
		m->frequency = 90000;
		m->payload = RTP_PAYLOAD_H264;
		snprintf(m->name, sizeof(m->name), "%s", "H264");
	}
	else if (MOV_OBJECT_HEVC == object)
	{
		assert(0);
		m->frequency = 90000;
		m->payload = RTP_PAYLOAD_H264;
		snprintf(m->name, sizeof(m->name), "%s", "H265");
	}
	else
	{
		assert(0);
		return;
	}
	
	struct rtp_payload_t rtpfunc = {
		MP4FileSource::RTPAlloc,
		MP4FileSource::RTPFree,
		MP4FileSource::RTPPacket,
	};
	m->packer = rtp_payload_encode_create(m->payload, m->name, (uint16_t)m->ssrc, m->ssrc, &rtpfunc, m);

	struct rtp_event_t event;
	event.on_rtcp = OnRTCPEvent;
	m->rtp = rtp_create(&event, self, m->ssrc, m->frequency, m->bandwidth);

	n += snprintf((char*)self->m_frame.buffer + n, sizeof(self->m_frame.buffer) - n, "a=control:track%d\n", m->track);
	self->m_sdp += (const char*)self->m_frame.buffer;
}

void MP4FileSource::MP4OnAudio(void* param, uint32_t track, uint8_t object, int channel_count, int /*bit_per_sample*/, int sample_rate, const void* extra, size_t bytes)
{
	int n = 0;
	MP4FileSource* self = (MP4FileSource*)param;
	struct media_t* m = &self->m_media[self->m_count++];
	m->track = track;
	m->rtcp_clock = 0;
	m->ssrc = (uint32_t)rtp_ssrc();
	m->timestamp = m->ssrc;
	m->bandwidth = 128 * 1024;
	m->dts_first = -1;
	m->dts_last = -1;

	if (MOV_OBJECT_AAC == object)
	{
		struct mpeg4_aac_t aac;
		//aac.profile = MPEG4_AAC_LC;
		//aac.channel_configuration = (uint8_t)channel_count;
		//aac.sampling_frequency_index = (uint8_t)mpeg4_aac_audio_frequency_from(sample_rate);
		mpeg4_aac_audio_specific_config_load((const uint8_t*)extra, bytes, &aac);
		assert(aac.sampling_frequency_index == (uint8_t)mpeg4_aac_audio_frequency_from(sample_rate));
		assert(aac.channel_configuration == channel_count);

		if (1)
		{
			// RFC 6416
			// In the presence of SBR, the sampling rates for the core encoder/
			// decoder and the SBR tool are different in most cases. Therefore,
			// this parameter SHALL NOT be considered as the definitive sampling rate.
			static const char* pattern =
				"m=audio 0 RTP/AVP %d\n"
				"a=rtpmap:%d MP4A-LATM/%d/%d\n"
				"a=fmtp:%d profile-level-id=%d;object=%d;cpresent=0;config=";

			sample_rate = 90000;
			n = snprintf((char*)self->m_frame.buffer, sizeof(self->m_frame.buffer), pattern,
				RTP_PAYLOAD_MP4A, RTP_PAYLOAD_MP4A, sample_rate, channel_count, 
				RTP_PAYLOAD_MP4A, mpeg4_aac_profile_level(&aac), aac.profile);

			uint8_t config[6];
			int r = mpeg4_aac_stream_mux_config_save(&aac, config, sizeof(config));
			static const char* hex = "0123456789abcdef";
			for (int i = 0; i < r; i++)
			{
				self->m_frame.buffer[n++] = hex[config[i] >> 4];
				self->m_frame.buffer[n++] = hex[config[i] & 0x0F];
			}
			self->m_frame.buffer[n] = '\0';

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

			n = snprintf((char*)self->m_frame.buffer, sizeof(self->m_frame.buffer), pattern,
				RTP_PAYLOAD_MP4A, RTP_PAYLOAD_MP4A, sample_rate, channel_count, RTP_PAYLOAD_MP4A);

			// For MPEG-4 Audio streams, config is the audio object type specific
			// decoder configuration data AudioSpecificConfig()
			n += base64_encode((char*)self->m_frame.buffer + n, extra, bytes);
			self->m_frame.buffer[n] = '\0';

			snprintf(m->name, sizeof(m->name), "%s", "MPEG4-GENERIC");
		}

		m->frequency = sample_rate;
		m->payload = RTP_PAYLOAD_MP4A;
		self->m_frame.buffer[n++] = '\n';
	}
	else
	{
		assert(0);
		return;
	}

	struct rtp_payload_t rtpfunc = {
		MP4FileSource::RTPAlloc,
		MP4FileSource::RTPFree,
		MP4FileSource::RTPPacket,
	};
	m->packer = rtp_payload_encode_create(m->payload, m->name, (uint16_t)m->ssrc, m->ssrc, &rtpfunc, m);
	
	struct rtp_event_t event;
	event.on_rtcp = OnRTCPEvent;
	m->rtp = rtp_create(&event, self, m->ssrc, m->frequency, m->bandwidth);

	n += snprintf((char*)self->m_frame.buffer + n, sizeof(self->m_frame.buffer) - n, "a=control:track%d\n", m->track);
	self->m_sdp += (const char*)self->m_frame.buffer;
}

void MP4FileSource::MP4OnRead(void* param, uint32_t track, const void* buffer, size_t bytes, int64_t pts, int64_t dts)
{
	struct frame_t* frame = (struct frame_t*)param;
	assert(frame->buffer == buffer);
	frame->track = track;
	frame->bytes = bytes;
	frame->pts = pts;
	frame->dts = dts;
}

void MP4FileSource::OnRTCPEvent(const struct rtcp_msg_t* msg)
{
	msg;
}

void MP4FileSource::OnRTCPEvent(void* param, const struct rtcp_msg_t* msg)
{
	MP4FileSource *self = (MP4FileSource *)param;
	self->OnRTCPEvent(msg);
}

int MP4FileSource::SendBye()
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

int MP4FileSource::SendRTCP(struct media_t* m, uint64_t clock)
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

void* MP4FileSource::RTPAlloc(void* param, int bytes)
{
	struct media_t* m = (struct media_t*)param;
	assert(bytes <= sizeof(m->packet));
	return m->packet;
}

void MP4FileSource::RTPFree(void* param, void *packet)
{
	struct media_t* m = (struct media_t*)param;
	assert(m->packet == packet);
}

void MP4FileSource::RTPPacket(void* param, const void *packet, int bytes, uint32_t /*timestamp*/, int /*flags*/)
{
	struct media_t* m = (struct media_t*)param;
	assert(m->packet == packet);

	int r = m->transport->Send(false, packet, bytes);
	assert(r == (int)bytes);
	rtp_onsend(m->rtp, packet, bytes/*, time*/);
}
