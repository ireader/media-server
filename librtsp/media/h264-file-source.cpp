#include "h264-file-source.h"
#include "cstringext.h"
#include "base64.h"
#include "rtp-profile.h"
#include "../payload/rtp-pack.h"
#include <assert.h>

extern "C" int rtp_ssrc(void);

H264FileSource::H264FileSource(const char *file)
:m_reader(file)
{
	m_speed = 1.0;
	m_status = 0;
	m_rtp_clock = 0;
	m_rtcp_clock = 0;

	unsigned int ssrc = (unsigned int)rtp_ssrc();
	static struct rtp_pack_func_t s_rtpfunc = {
		H264FileSource::RTPAlloc,
		H264FileSource::RTPFree,
		H264FileSource::RTPPacket,
	};
	m_rtppacker = rtp_h264_packer()->create(ssrc, (unsigned short)ssrc, RTP_PAYLOAD_H264, &s_rtpfunc, this);

	struct rtp_event_t event;
	event.on_rtcp = OnRTCPEvent;
	m_rtp = rtp_create(&event, this, ssrc, 90000, 4*1024);
	rtp_set_info(m_rtp, "RTSPServer", "szj.h264");
}

H264FileSource::~H264FileSource()
{
	if(m_rtp)
		rtp_destroy(m_rtp);

	if(m_rtppacker)
		rtp_h264_packer()->destroy(m_rtppacker);
}

H264FileSource* H264FileSource::Create(const char *file)
{
	H264FileSource* s = new H264FileSource(file);
	return s;
}

int H264FileSource::SetRTPSocket(const char* ip, socket_t socket[2], unsigned short port[2])
{
	m_socket[0] = socket[0];
	m_socket[1] = socket[1];
	m_port[0] = port[0];
	m_port[1] = port[1];
	m_ip.assign(ip);
	return 0;
}

int H264FileSource::Play()
{
	m_status = 1;

	time64_t clock = time64_now();
	if(0 == m_rtp_clock || m_rtp_clock + 40 < clock)
	{
		void* ptr = NULL;
		size_t bytes = 0;
		if(0 == m_reader.GetNextFrame(m_pos, ptr, bytes))
		{
			rtp_h264_packer()->input(m_rtppacker, ptr, bytes, clock);
			m_rtp_clock = clock;

			SendRTCP();
			return 1;
		}
	}

	return 0;
}

int H264FileSource::Pause()
{
	m_status = 2;
	return 0;
}

int H264FileSource::Seek(int64_t pos)
{
	m_pos = pos;
	return m_reader.Seek(m_pos);
}

int H264FileSource::SetSpeed(double speed)
{
	m_speed = speed;
	return 0;
}

int H264FileSource::GetDuration(int64_t& duration) const
{
	return m_reader.GetDuration(duration);
}

int H264FileSource::GetSDPMedia(std::string& sdp) const
{
    static const char* pattern =
        "m=video 0 RTP/AVP %d\n"
        "a=rtpmap:%d H264/90000\n"
        "a=fmtp:%d profile-level-id=%02X%02X%02X;"
    			 "packetization-mode=1;"
    			 "sprop-parameter-sets=";

    char base64[512] = {0};
    std::string parameters;

    const std::list<H264FileReader::sps_t>& sps = m_reader.GetParameterSets();
    std::list<H264FileReader::sps_t>::const_iterator it;
    for(it = sps.begin(); it != sps.end(); ++it)
    {
        if(parameters.empty())
        {
            snprintf(base64, sizeof(base64), pattern, 
				RTP_PAYLOAD_H264, RTP_PAYLOAD_H264,RTP_PAYLOAD_H264, 
				(unsigned int)(*it)[1], (unsigned int)(*it)[2], (unsigned int)(*it)[3]);
            sdp = base64;
        }
        else
        {
            parameters += ',';
        }

        size_t bytes = it->size();
        assert((bytes+2)/3*4 + bytes/57 + 1 < sizeof(base64));
        bytes = base64_encode(base64, &(*it)[0], bytes);
		base64[bytes] = '\0';
        assert(strlen(base64) > 0);
        parameters += base64;
    }

    sdp += parameters;
    sdp += '\n';
    return sps.empty() ? -1 : 0;
}

int H264FileSource::GetRTPInfo(int64_t &pos, unsigned short &seq, unsigned int &rtptime) const
{
	rtp_h264_packer()->get_info(m_rtppacker, &seq, &rtptime);
	pos = m_pos;
	return 0;
}

void H264FileSource::OnRTCPEvent(const struct rtcp_msg_t* msg)
{
	msg;
}

void H264FileSource::OnRTCPEvent(void* param, const struct rtcp_msg_t* msg)
{
	H264FileSource *self = (H264FileSource *)param;
	self->OnRTCPEvent(msg);
}

int H264FileSource::SendRTCP()
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

void* H264FileSource::RTPAlloc(void* param, size_t bytes)
{
	H264FileSource *self = (H264FileSource*)param;
	assert(bytes <= sizeof(self->m_packet));
	return self->m_packet;
}

void H264FileSource::RTPFree(void* param, void *packet)
{
	H264FileSource *self = (H264FileSource*)param;
	assert(self->m_packet == packet);
}

void H264FileSource::RTPPacket(void* param, void *packet, size_t bytes, int64_t time)
{
	H264FileSource *self = (H264FileSource*)param;
	assert(self->m_packet == packet);

	struct sockaddr_in addrin;
	socket_addr_ipv4(&addrin, self->m_ip.c_str(), self->m_port[0]);
	int r = socket_sendto(self->m_socket[0], packet, bytes, 0, (struct sockaddr*)&addrin, sizeof(addrin));
	assert(r == (int)bytes);
	rtp_onsend(self->m_rtp, packet, bytes, time);
}
