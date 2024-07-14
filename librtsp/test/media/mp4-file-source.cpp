#include "mp4-file-source.h"
#include "mov-format.h"
#include "rtp.h"
#include "rtp-profile.h"
#include "rtp-payload.h"
#include "sys/system.h"
#include "sys/path.h"
#include <assert.h>

extern "C" const struct mov_buffer_t* mov_file_buffer(void);

MP4FileSource::MP4FileSource(const char *file)
{
	m_speed = 1.0;
	m_status = 0;
	m_clock = 0;
	m_count = 0;
	m_dts = -1;

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
		rtp_set_info(m->rtp.rtp, "RTSPServer", path_basename(file));
	}
}

MP4FileSource::~MP4FileSource()
{
	for (int i = 0; i < m_count; i++)
	{
		struct media_t* m = &m_media[i];

		rtp_sender_destroy(&m->rtp);

		if (m->pkts)
		{
			avpacket_queue_destroy(m->pkts);
			m->pkts = NULL;
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

struct MP4FileSource::media_t* MP4FileSource::FetchNextPacket()
{
	struct media_t* best = NULL;
	int64_t timestamp = 0;

	for (int i = 0; i < m_count; i++)
	{
		struct media_t* m = &m_media[i];
		while (0 == avpacket_queue_count(m->pkts))
		{
			int r = mov_reader_read(m_reader, m_packet, sizeof(m_packet), MP4OnRead, this);
			if (r == 0)
			{
				// 0-EOF
				break;
			}
			else if (r < 0)
			{
				// error
				return NULL;
			}
		}

		if(0 == avpacket_queue_count(m->pkts))
			continue;

		std::shared_ptr<struct avpacket_t> cur(avpacket_queue_front(m->pkts), avpacket_release);
		if (NULL == best || cur->dts < timestamp || cur->pts < timestamp)
		{
			best = m;
			timestamp = cur->dts < cur->pts ? cur->dts : cur->pts;
		}
	}

	//m_status = 3;
	//SendBye();
	return best;
}

int MP4FileSource::Play()
{
	bool sendframe = false;
	if (3 == m_status)
		return 0;

	struct media_t* m = FetchNextPacket();
	if (NULL == m)
	{
		// 0-EOF
		m_status = 3;
		SendBye();
		return 0;
	}

	m_status = 1;
	int bytes = 0;
	uint64_t clock = system_clock();
	std::shared_ptr<struct avpacket_t> pkt(avpacket_queue_front(m->pkts), avpacket_release);
	int64_t dts = pkt->dts < pkt->pts ? pkt->dts : pkt->pts;

	SendRTCP(clock);
	if (0 == m_clock || m_clock > clock)
		m_clock = clock;
	if (-1 == m_dts)
		m_dts = dts;

	if (int64_t(clock - m_clock) + m_dts >= dts)
	{
		if (0 == strcmp("H264", m->rtp.encoding))
		{
			// AVC1 -> H.264 byte stream
			bytes = h264_mp4toannexb(&m_avc, pkt->data, pkt->size, m_packet, sizeof(m_packet));
			//printf("[V] pts: %lld, dts: %lld, clock: %llu\n", m_frame.pts, m_frame.dts, clock);
		}
		else if (0 == strcmp("H265", m->rtp.encoding))
		{
			// HVC1 -> H.264 byte stream
			bytes = h265_mp4toannexb(&m_hevc, pkt->data, pkt->size, m_packet, sizeof(m_packet));
			//printf("[V] pts: %lld, dts: %lld, clock: %llu\n", m_frame.pts, m_frame.dts, clock);
		}
		else if (0 == strcmp("MP4A-LATM", m->rtp.encoding) || 0 == strcmp("MPEG4-GENERIC", m->rtp.encoding))
		{
			// add ADTS header
			memcpy(m_packet, pkt->data, pkt->size);
			bytes = pkt->size;
			//printf("[A] pts: %lld, dts: %lld, clock: %llu\n", m_frame.pts, m_frame.dts, clock);
		}
		else
		{
			assert(0);
		}

		if (-1 == m->dts_first)
			m->dts_first = pkt->pts;
		m->dts_last = pkt->pts;
		uint32_t timestamp = m->rtp.timestamp + (uint32_t)((m->dts_last - m->dts_first) * (m->rtp.frequency / 1000) /*kHz*/);
		//printf("[%s] pts: %lld, dts: %lld, timestamp: %u(%u)\n", m->rtp.encoding, pkt->pts, pkt->dts, (unsigned int)timestamp, (unsigned int)m->timestamp);
		rtp_payload_encode_input(m->rtp.encoder, m_packet, bytes, timestamp);

		avpacket_queue_pop(m->pkts);
		sendframe = 1;

		// preload next packet
		FetchNextPacket();
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
			m_media[i].rtp.timestamp += (uint32_t)(m_media[i].dts_last - m_media[i].dts_first + 1);
		m_media[i].dts_first = -1;
		avpacket_queue_clear(m_media[i].pkts); // clear buffered frame
	}

	m_dts = pos;
	m_clock = 0;
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
		rtp_payload_encode_getinfo(m->rtp.encoder, &seq, &timestamp);

		if (i > 0)
			rtpinfo[n++] = ',';
		n += snprintf(rtpinfo + n, bytes - n, "url=%s/track%d;seq=%hu;rtptime=%u", uri, m->track, seq, (unsigned int)m->rtp.timestamp);
	}
	return 0;
}

void MP4FileSource::MP4OnVideo(void* param, uint32_t track, uint8_t object, int /*width*/, int /*height*/, const void* extra, size_t bytes)
{
	assert(track > 0);
	int n = 0;
	MP4FileSource* self = (MP4FileSource*)param;
	struct media_t* m = &self->m_media[self->m_count++];
	memset(&m->rtp, 0, sizeof(m->rtp));
	m->pkts = avpacket_queue_create(100);
	m->track = track;
	m->rtcp_clock = 0;
	m->dts_first = -1;
	m->dts_last = -1;

	if (MOV_OBJECT_H264 == object)
	{
		mpeg4_avc_decoder_configuration_record_load((const uint8_t*)extra, bytes, &self->m_avc);
		n = rtp_sender_init_video(&m->rtp, "RTP/AVP", 0, RTP_PAYLOAD_H264, "H264", 90000, extra, bytes);
	}
	else if (MOV_OBJECT_HEVC == object)
	{
		mpeg4_hevc_decoder_configuration_record_load((const uint8_t*)extra, bytes, &self->m_hevc);
		n = rtp_sender_init_video(&m->rtp, "RTP/AVP", 0, RTP_PAYLOAD_H265, "H265", 90000, extra, bytes);
	}
	else if (MOV_OBJECT_MP4V == object)
	{
		n = rtp_sender_init_video(&m->rtp, "RTP/AVP", 0, RTP_PAYLOAD_MP4V, "MP4V-ES", 90000, extra, bytes);
	}
	else
	{
		assert(0);
		return;
	}

	m->rtp.onpacket = OnRTPPacket;
	m->rtp.param = m;
	n = snprintf((char*)self->m_packet, sizeof(self->m_packet), "%.*sa=control:track%d\n", n, m->rtp.buffer, m->track);
	self->m_sdp += (const char*)self->m_packet;
}

void MP4FileSource::MP4OnAudio(void* param, uint32_t track, uint8_t object, int channel_count, int /*bit_per_sample*/, int sample_rate, const void* extra, size_t bytes)
{
	assert(track > 0);
	int n = 0;
	MP4FileSource* self = (MP4FileSource*)param;
	struct media_t* m = &self->m_media[self->m_count++];
	memset(&m->rtp, 0, sizeof(m->rtp));
	m->pkts = avpacket_queue_create(100);
	m->track = track;
	m->rtcp_clock = 0;
	m->dts_first = -1;
	m->dts_last = -1;

	if (MOV_OBJECT_AAC == object || MOV_OBJECT_AAC_LOW == object)
	{
		mpeg4_aac_audio_specific_config_load((const uint8_t*)extra, bytes, &self->m_aac);

		if (1)
		{
			// RFC 6416
			n = rtp_sender_init_audio(&m->rtp, "RTP/AVP", 0, RTP_PAYLOAD_LATM, "MP4A-LATM", sample_rate, self->m_aac.channel_configuration, extra, bytes);
		}
		else
		{
			// RFC 3640 3.3.1. General (p21)
			n = rtp_sender_init_audio(&m->rtp, "RTP/AVP", 0, RTP_PAYLOAD_MP4A, "MP4A-GENERIC", sample_rate, channel_count, extra, bytes);
		}
	}
	else if (MOV_OBJECT_OPUS == object)
	{
		// RFC7587 RTP Payload Format for the Opus Speech and Audio Codec
		n = rtp_sender_init_audio(&m->rtp, "RTP/AVP", 0, RTP_PAYLOAD_OPUS, "opus", sample_rate, channel_count, extra, bytes);
	}
	else if (MOV_OBJECT_G711u == object)
	{
		n = rtp_sender_init_audio(&m->rtp, "RTP/AVP", 0, RTP_PAYLOAD_PCMU, "PCMU", 8000, 1, extra, bytes);
	}
	else if (MOV_OBJECT_G711a == object)
	{
		n = rtp_sender_init_audio(&m->rtp, "RTP/AVP", 0, RTP_PAYLOAD_PCMA, "PCMA", 8000, 1, extra, bytes);
	}
	else
	{
		assert(0);
		return;
	}

	m->rtp.param = m;
	m->rtp.onpacket = OnRTPPacket;
	n = snprintf((char*)self->m_packet, sizeof(self->m_packet), "%.*sa=control:track%d\n", n, m->rtp.buffer, m->track);
	self->m_sdp += (const char*)self->m_packet;
}

void MP4FileSource::MP4OnRead(void* param, uint32_t track, const void* buffer, size_t bytes, int64_t pts, int64_t dts, int /*flags*/)
{
	MP4FileSource *self = (MP4FileSource *)param;

	std::shared_ptr<struct avpacket_t> pkt(avpacket_alloc((int)bytes), avpacket_release);
	memcpy(pkt->data, buffer, bytes);
	//pkt->codecid = track;
	pkt->pts = pts;
	pkt->dts = dts;

	for (int i = 0; i < self->m_count; i++)
	{
		struct media_t* m = &self->m_media[i];
		if (m->track == (int)track)
		{
			// TODO: overflow
			assert(0 == avpacket_queue_push(m->pkts, pkt.get()));
			//assert(0 == avpacket_queue_push_wait(m->pkts, &pkt, 1000000));
			break;
		}
	}
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
		
		size_t n = rtp_rtcp_bye(m->rtp.rtp, rtcp, sizeof(rtcp));

		// send RTCP packet
		m->transport->Send(true, rtcp, n);
	}

	return 0;
}

int MP4FileSource::SendRTCP(uint64_t clock)
{
	char rtcp[1024] = { 0 };

	// make sure have sent RTP packet
	for (int i = 0; i < m_count; i++)
	{
		struct media_t* m = &m_media[i];
		int interval = rtp_rtcp_interval(m->rtp.rtp);
		if (0 == m->rtcp_clock || m->rtcp_clock + interval < clock)
		{
			size_t n = rtp_rtcp_report(m->rtp.rtp, rtcp, sizeof(rtcp));

			// send RTCP packet
			m->transport->Send(true, rtcp, n);

			m->rtcp_clock = clock;
		}
	}

	return 0;
}

int MP4FileSource::OnRTPPacket(void* param, const void *packet, int bytes, uint32_t /*timestamp*/, int /*flags*/)
{
	struct media_t* m = (struct media_t*)param;
	int r = m->transport->Send(false, packet, bytes);
	if (r != bytes)
	{
		assert(0);
		return -1;
	}	

	return rtp_onsend(m->rtp.rtp, packet, bytes/*, time*/);
}
