#include "ps-file-source.h"
#include "cstringext.h"
#include "mpeg-ps.h"
#include <assert.h>

#define MAX_UDP_PACKET (1450-16)

extern "C" int rtp_ssrc(void);

PSFileSource::PSFileSource(const char *file)
:m_reader(file)
{
	m_status = 0;
	m_rtp_clock = 0;
	m_rtcp_clock = 0;
	m_timestamp = 0;
	m_ssrc = (unsigned int)rtp_ssrc();
	m_seq = (unsigned short)m_ssrc;

	struct mpeg_ps_func_t func;
	func.alloc = Alloc;
	func.free = Free;
	func.write = Packet;
	m_ps = mpeg_ps_create(&func, this);
	mpeg_ps_add_stream(m_ps, STREAM_VIDEO_H264, NULL, 0);

	struct rtp_event_t event;
	event.on_rtcp = OnRTCPEvent;
	m_rtp = rtp_create(&event, this, m_ssrc, 90000, 4*1024);
	rtp_set_info(m_rtp, "RTSPServer", "szj.h264");
}

PSFileSource::~PSFileSource()
{
	if(m_rtp)
		rtp_destroy(m_rtp);
	mpeg_ps_destroy(m_ps);
}

PSFileSource* PSFileSource::Create(const char *file)
{
	PSFileSource* s = new PSFileSource(file);
	return s;
}

int PSFileSource::SetRTPSocket(const char* ip, socket_t socket[2], unsigned short port[2])
{
	m_socket[0] = socket[0];
	m_socket[1] = socket[1];
	m_port[0] = port[0];
	m_port[1] = port[1];
	m_ip.assign(ip);
	return 0;
}

int PSFileSource::Play()
{
	m_status = 1;

	time64_t clock = time64_now();
	if(0 == m_rtp_clock || m_rtp_clock + 40 < clock)
	{
		void* ptr = NULL;
		size_t bytes = 0;
		if(0 == m_reader.GetNextFrame(ptr, bytes))
		{
			m_rtp_clock = clock;
//			Pack(ptr, bytes);
			mpeg_ps_write(m_ps, STREAM_VIDEO_H264, m_timestamp, m_timestamp, ptr, bytes);
			m_timestamp += 3600;

			SendRTCP();
			return 1;
		}
	}

	return 0;
}

int PSFileSource::Pause()
{
	m_status = 2;
	return 0;
}

int PSFileSource::Seek(int64_t pos)
{
	return m_reader.Seek(pos);
}

int PSFileSource::SetSpeed(double speed)
{
	return 0;
}

int PSFileSource::GetDuration(int64_t& duration) const
{
	return m_reader.GetDuration(duration);
}

int PSFileSource::GetSDPMedia(std::string& sdp) const
{
	static const char* pattern =
		"m=video 0 RTP/AVP 96\n"
		"a=rtpmap:96 MP2P/90000\n";
	
	sdp = pattern;
	return 0;
}

int PSFileSource::GetRTPInfo(int64_t &pos, unsigned short &seq, unsigned int &rtptime) const
{
	return 0;
}

void PSFileSource::OnRTCPEvent(const struct rtcp_msg_t* msg)
{
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
		struct sockaddr_in addrin;
		socket_addr_ipv4(&addrin, m_ip.c_str(), m_port[1]);
		socket_sendto(m_socket[1], rtcp, n, 0, (struct sockaddr*)&addrin, sizeof(addrin));

		m_rtcp_clock = clock;
	}

	return 0;
}

void* PSFileSource::Alloc(void* param, size_t bytes)
{
	PSFileSource* self = (PSFileSource*)param;
	self;
	return malloc(bytes);
}

void PSFileSource::Free(void* param, void* packet)
{
	PSFileSource* self = (PSFileSource*)param;
	self;
	return free(packet);
}

void PSFileSource::Packet(void* param, void* pes, size_t bytes)
{
	PSFileSource* self = (PSFileSource*)param;

	const uint8_t *p;
	unsigned char rtp[MAX_UDP_PACKET+14];

	rtp[0] = (unsigned char)(0x80);
	rtp[1] = (unsigned char)(96);

	rtp[4] = (unsigned char)(self->m_timestamp >> 24);
	rtp[5] = (unsigned char)(self->m_timestamp >> 16);
	rtp[6] = (unsigned char)(self->m_timestamp >> 8);
	rtp[7] = (unsigned char)(self->m_timestamp);

	rtp[8] = (unsigned char)(self->m_ssrc >> 24);
	rtp[9] = (unsigned char)(self->m_ssrc >> 16);
	rtp[10] = (unsigned char)(self->m_ssrc >> 8);
	rtp[11] = (unsigned char)(self->m_ssrc);

	p = (const unsigned char *)pes;
	while(bytes > 0)
	{
		size_t len;

		assert(0 == (rtp[1] & 0x80)); // don't set market
		if(bytes <= MAX_UDP_PACKET)
			rtp[1] |= 0x80; // set marker flag
		rtp[2] = (unsigned char)(self->m_seq >> 8);
		rtp[3] = (unsigned char)(self->m_seq);
		++self->m_seq;

		len = bytes > MAX_UDP_PACKET ? MAX_UDP_PACKET : bytes;
		memcpy(rtp+12, p, len);

		struct sockaddr_in addrin;
		socket_addr_ipv4(&addrin, self->m_ip.c_str(), self->m_port[0]);
		int r = socket_sendto(self->m_socket[0], rtp, len+12, 0, (struct sockaddr*)&addrin, sizeof(addrin));
		assert(r == len+12);
		rtp_onsend(self->m_rtp, rtp, len+12, self->m_rtcp_clock);

		p += len;
		bytes -= len;
	}

	free(pes);
}
