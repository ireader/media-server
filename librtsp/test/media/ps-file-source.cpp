#include "ps-file-source.h"
#include "cstringext.h"
#include "rtp-profile.h"
#include "rtp-payload.h"
#include <assert.h>

extern "C" uint32_t rtp_ssrc(void);

PSFileSource::PSFileSource(const char *file)
:m_reader(file)
{
	m_speed = 1.0;
	m_status = 0;
	m_ps_clock = 0;
	m_rtp_clock = 0;
	m_rtcp_clock = 0;

	uint32_t ssrc = rtp_ssrc();

	struct ps_muxer_func_t func;
	func.alloc = Alloc;
	func.free = Free;
	func.write = Packet;
	m_ps = ps_muxer_create(&func, this);
    m_ps_stream = ps_muxer_add_stream(m_ps, STREAM_VIDEO_H264, NULL, 0);

	static struct rtp_payload_t s_psfunc = {
		PSFileSource::RTPAlloc,
		PSFileSource::RTPFree,
		PSFileSource::RTPPacket,
	};
	m_pspacker = rtp_payload_encode_create(RTP_PAYLOAD_MP2P, "MP2P", (uint16_t)ssrc, ssrc, &s_psfunc, this);

	struct rtp_event_t event;
	event.on_rtcp = OnRTCPEvent;
	m_rtp = rtp_create(&event, this, ssrc, ssrc, 90000, 4*1024, 1);
	rtp_set_info(m_rtp, "RTSPServer", "szj.h264");
}

PSFileSource::~PSFileSource()
{
	if(m_rtp)
		rtp_destroy(m_rtp);
	if(m_pspacker)
		rtp_payload_encode_destroy(m_pspacker);
	ps_muxer_destroy(m_ps);
}

int PSFileSource::SetTransport(const char* /*track*/, std::shared_ptr<IRTPTransport> transport)
{
	m_transport = transport;
	return 0;
}

int PSFileSource::Play()
{
	m_status = 1;

	time64_t clock = time64_now();
	if(0 == m_rtp_clock || m_rtp_clock + 40 < (clock - m_ps_clock))
	{
		size_t bytes;
		const uint8_t* ptr;
		if(0 == m_reader.GetNextFrame(m_pos, ptr, bytes))
		{
			if(0 == m_ps_clock)
				m_ps_clock = clock;
			ps_muxer_input(m_ps, m_ps_stream, 0, (clock-m_ps_clock)*90, (clock-m_ps_clock)*90, ptr, bytes);
			m_rtp_clock += 40;

			SendRTCP();
			return 1;
		}
	}

	return 0;
}

int PSFileSource::Pause()
{
	m_status = 2;
	m_rtp_clock = 0;
	return 0;
}

int PSFileSource::Seek(int64_t pos)
{
	m_rtp_clock = 0;
	return m_reader.Seek(pos);
}

int PSFileSource::SetSpeed(double speed)
{
	m_speed = speed;
	return 0;
}

int PSFileSource::GetDuration(int64_t& duration) const
{
	return m_reader.GetDuration(duration);
}

int PSFileSource::GetSDPMedia(std::string& sdp) const
{
	static const char* pattern =
		"m=video 0 RTP/AVP %d\n"
		"a=rtpmap:%d MP2P/90000\n";
	
	char media[64];
	snprintf(media, sizeof(media), pattern, RTP_PAYLOAD_MP2P, RTP_PAYLOAD_MP2P);
	sdp = media;
	return 0;
}

int PSFileSource::GetRTPInfo(const char* uri, char *rtpinfo, size_t bytes) const
{
	uint16_t seq;
	uint32_t timestamp;
	rtp_payload_encode_getinfo(m_pspacker, &seq, &timestamp);

	// url=rtsp://video.example.com/twister/video;seq=12312232;rtptime=78712811
	snprintf(rtpinfo, bytes, "url=%s;seq=%hu;rtptime=%u", uri, seq, timestamp);
	return 0;
}

void PSFileSource::OnRTCPEvent(const struct rtcp_msg_t* msg)
{
	msg;
}

void PSFileSource::OnRTCPEvent(void* param, const struct rtcp_msg_t* msg)
{
	PSFileSource *self = (PSFileSource *)param;
	self->OnRTCPEvent(msg);
}

int PSFileSource::SendRTCP()
{
	// make sure have sent RTP packet

	time64_t clock = time64_now();
	int interval = rtp_rtcp_interval(m_rtp);
	if(0 == m_rtcp_clock || m_rtcp_clock + interval < clock)
	{
		char rtcp[1024] = {0};
		size_t n = rtp_rtcp_report(m_rtp, rtcp, sizeof(rtcp));

		// send RTCP packet
		m_transport->Send(true, rtcp, n);

		m_rtcp_clock = clock;
	}

	return 0;
}

void* PSFileSource::Alloc(void* /*param*/, size_t bytes)
{
//	PSFileSource* self = (PSFileSource*)param;
	return malloc(bytes);
}

void PSFileSource::Free(void* /*param*/, void* packet)
{
//	PSFileSource* self = (PSFileSource*)param;
	return free(packet);
}

void PSFileSource::Packet(void* param, int /*avtype*/, void* pes, size_t bytes)
{
	PSFileSource* self = (PSFileSource*)param;
	time64_t clock = time64_now();
	rtp_payload_encode_input(self->m_pspacker, pes, bytes, clock * 90 /*kHz*/);
}

void* PSFileSource::RTPAlloc(void* param, int bytes)
{
	PSFileSource *self = (PSFileSource*)param;
	assert(bytes <= sizeof(self->m_packet));
	return self->m_packet;
}

void PSFileSource::RTPFree(void* param, void *packet)
{
	PSFileSource *self = (PSFileSource*)param;
	assert(self->m_packet == packet);
}

void PSFileSource::RTPPacket(void* param, const void *packet, int bytes, uint32_t /*timestamp*/, int /*flags*/)
{
	PSFileSource *self = (PSFileSource*)param;
	assert(self->m_packet == packet);

	int r = self->m_transport->Send(false, packet, bytes);
	assert(r == (int)bytes);
	rtp_onsend(self->m_rtp, packet, bytes/*, time*/);
}
