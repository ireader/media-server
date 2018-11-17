#include "mp4-file-source.h"
#include "mov-format.h"
#include "rtp.h"
#include "rtp-profile.h"
#include "rtp-payload.h"
#include "sys/system.h"
#include "sys/path.h"
#include <assert.h>

extern "C" int rtp_ssrc(void);
extern "C" const struct mov_buffer_t* mov_file_buffer(void);
extern "C" int sdp_h264(uint8_t *data, int bytes, int payload, int frequence, const void* extra, int extra_size);
extern "C" int sdp_h265(uint8_t *data, int bytes, int payload, int frequence, const void* extra, int extra_size);
extern "C" int sdp_aac_latm(uint8_t *data, int bytes, int payload, int sample_rate, int channel_count, const void* extra, int extra_size);
extern "C" int sdp_aac_generic(uint8_t *data, int bytes, int payload, int sample_rate, int channel_count, const void* extra, int extra_size);
extern "C" int sdp_opus(uint8_t *data, int bytes, int payload, int sample_rate, int channel_count, const void* extra, int extra_size);
extern "C" int sdp_g711u(uint8_t *data, int bytes, int payload, int sample_rate, int channel_count, const void* extra, int extra_size);

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
		{
			if (i + 1 == m_count)
			{
				m_frame.bytes = 0; // unknown track, ignore
				goto SEND_PACKET;
			}
			continue;
		}

		if (0 == m_clock || m_clock > clock)
			m_clock = clock;
		if (-1 == m_dts)
			m_dts = m_frame.dts;

		if (int64_t(clock - m_clock) + m_dts >= m_frame.dts)
		{
			if (0 == strcmp("H264", m->name))
			{
				// AVC1 -> H.264 byte stream
				m_frame.bytes = mpeg4_mp4toannexb(&m_avc, m_frame.buffer, m_frame.bytes, m_packet, sizeof(m_packet));
				//printf("[V] pts: %lld, dts: %lld, clock: %llu\n", m_frame.pts, m_frame.dts, clock);
			}
			else if (0 == strcmp("H265", m->name))
			{
				// HVC1 -> H.264 byte stream
				m_frame.bytes = hevc_mp4toannexb(&m_hevc, m_frame.buffer, m_frame.bytes, m_packet, sizeof(m_packet));
				//printf("[V] pts: %lld, dts: %lld, clock: %llu\n", m_frame.pts, m_frame.dts, clock);
			}
			else if (0 == strcmp("MP4A-LATM", m->name) || 0 == strcmp("MPEG4-GENERIC", m->name))
			{
				// add ADTS header
				memcpy(m_packet, m_frame.buffer, m_frame.bytes);
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
			rtp_payload_encode_input(m->packer, m_packet, m_frame.bytes, (uint32_t)(timestamp * (m->frequency / 1000) /*kHz*/));
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
		mpeg4_avc_decoder_configuration_record_load((const uint8_t*)extra, bytes, &self->m_avc);
		m->frequency = 90000;
		m->payload = RTP_PAYLOAD_H264;
		snprintf(m->name, sizeof(m->name), "%s", "H264");
		n = sdp_h264(self->m_frame.buffer, sizeof(self->m_frame.buffer), RTP_PAYLOAD_H264, 90000, extra, bytes);
	}
	else if (MOV_OBJECT_HEVC == object)
	{
		mpeg4_hevc_decoder_configuration_record_load((const uint8_t*)extra, bytes, &self->m_hevc);
		m->frequency = 90000;
		m->payload = RTP_PAYLOAD_H265;
		snprintf(m->name, sizeof(m->name), "%s", "H265");
		n = sdp_h265(self->m_frame.buffer, sizeof(self->m_frame.buffer), RTP_PAYLOAD_H265, 90000, extra, bytes);
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

	if (MOV_OBJECT_AAC == object || MOV_OBJECT_AAC_LOW == object)
	{
		mpeg4_aac_audio_specific_config_load((const uint8_t*)extra, bytes, &self->m_aac);

		if (1)
		{
			// RFC 6416
			m->frequency = sample_rate;
			m->payload = RTP_PAYLOAD_MP4A;
			snprintf(m->name, sizeof(m->name), "%s", "MP4A-LATM");
			n = sdp_aac_latm(self->m_frame.buffer, sizeof(self->m_frame.buffer), RTP_PAYLOAD_MP4A, sample_rate, channel_count, extra, bytes);
		}
		else
		{
			// RFC 3640 3.3.1. General (p21)
			m->frequency = sample_rate;
			m->payload = RTP_PAYLOAD_MP4A;
			snprintf(m->name, sizeof(m->name), "%s", "MPEG4-GENERIC"); 
			n = sdp_aac_generic(self->m_frame.buffer, sizeof(self->m_frame.buffer), RTP_PAYLOAD_MP4A, sample_rate, channel_count, extra, bytes);
		}
	}
	else if (MOV_OBJECT_OPUS == object)
	{
		// RFC7587 RTP Payload Format for the Opus Speech and Audio Codec
		m->frequency = sample_rate;
		m->payload = RTP_PAYLOAD_OPUS;
		snprintf(m->name, sizeof(m->name), "%s", "opus");
		n = sdp_opus(self->m_frame.buffer, sizeof(self->m_frame.buffer), RTP_PAYLOAD_OPUS, sample_rate, channel_count, extra, bytes);
	}
	else if (MOV_OBJECT_G711u == object)
	{
		m->frequency = sample_rate;
		m->payload = RTP_PAYLOAD_PCMU;
		snprintf(m->name, sizeof(m->name), "%s", "PCMU");
		n = sdp_g711u(self->m_frame.buffer, sizeof(self->m_frame.buffer), RTP_PAYLOAD_PCMU, sample_rate, channel_count, extra, bytes);
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
