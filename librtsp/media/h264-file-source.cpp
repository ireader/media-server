#include "h264-file-source.h"
#include "cstringext.h"
#include "base64.h"
#include <assert.h>

#define MAX_UDP_PACKET (1450-16)

extern "C" int rtp_ssrc(void);

H264FileSource::H264FileSource(const char *file)
:m_reader(file)
{
	m_status = 0;
	m_rtp_clock = 0;
	m_rtcp_clock = 0;
	m_timestamp = 0;
	m_ssrc = (unsigned int)rtp_ssrc();
	m_seq = (unsigned short)m_ssrc;

	struct rtp_event_t event;
	event.on_rtcp = OnRTCPEvent;
	m_rtp = rtp_create(&event, this, m_ssrc, 90000, 4*1024);
	rtp_set_info(m_rtp, "RTSPServer", "szj.h264");
}

H264FileSource::~H264FileSource()
{
	if(m_rtp)
		rtp_destroy(m_rtp);
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
		if(0 == m_reader.GetNextFrame(ptr, bytes))
		{
			Pack(ptr, bytes);
			m_rtp_clock = clock;
			m_timestamp += 3600;

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
	return m_reader.Seek(pos);
}

int H264FileSource::SetSpeed(double speed)
{
	return 0;
}

int H264FileSource::GetDuration(int64_t& duration) const
{
	return m_reader.GetDuration(duration);
}

int H264FileSource::GetSDPMedia(std::string& sdp) const
{
    static const char* pattern =
        "m=video 0 RTP/AVP 98\n"
        "a=rtpmap:98 H264/90000\n"
        "a=fmtp:98 profile-level-id=%02X%02X%02X;"
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
            snprintf(base64, sizeof(base64), pattern, (unsigned int)(*it)[1], (unsigned int)(*it)[2], (unsigned int)(*it)[3]);
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
	return 0;
}

void H264FileSource::OnRTCPEvent(const struct rtcp_msg_t* msg)
{
}

void H264FileSource::OnRTCPEvent(void* param, const struct rtcp_msg_t* msg)
{
	H264FileSource *self = (H264FileSource *)param;
	self->OnRTCPEvent(msg);
}

inline const unsigned char* search_start_code(const unsigned char* ptr, size_t bytes)
{
	const unsigned char *p;
	for(p = ptr; p + 3 < ptr + bytes; p++)
	{
		if(0x00 == p[0] && 0x00 == p[1] && (0x01 == p[2] || (0x00==p[2] && 0x01==p[3])))
			return p;
	}
	return NULL;
}

int H264FileSource::Pack(const void* h264, size_t bytes)
{
	int r;
	const unsigned char *p1, *p2;
	unsigned char packet[MAX_UDP_PACKET+14];

	p1 = (const unsigned char *)h264;
	assert(p1 == search_start_code(p1, bytes));

	packet[0] = (unsigned char)(0x80);
	packet[1] = (unsigned char)(98);

	packet[4] = (unsigned char)(m_timestamp >> 24);
	packet[5] = (unsigned char)(m_timestamp >> 16);
	packet[6] = (unsigned char)(m_timestamp >> 8);
	packet[7] = (unsigned char)(m_timestamp);

	packet[8] = (unsigned char)(m_ssrc >> 24);
	packet[9] = (unsigned char)(m_ssrc >> 16);
	packet[10] = (unsigned char)(m_ssrc >> 8);
	packet[11] = (unsigned char)(m_ssrc);

	time64_t clock = time64_now();

	while(bytes > 0)
	{
		size_t nalu_size;

		p2 = search_start_code(p1+3, bytes - 3);
		if(!p2) p2 = p1 + bytes;
		nalu_size = p2 - p1;
		bytes -= nalu_size;

		// filter suffix '00' bytes
		while(0 == p2[nalu_size-1]) --nalu_size;

		// filter H.264 start code(0x00000001)
		nalu_size -= (0x01 == p1[2]) ? 3 : 4;
		p1 += (0x01 == p1[2]) ? 3 : 4;
		assert(0 < (*p1 & 0x1F) && (*p1 & 0x1F) < 24);

		if(nalu_size < MAX_UDP_PACKET)
		{
			packet[1] |= 0x80; // marker
			packet[2] = (unsigned char)(m_seq >> 8);
			packet[3] = (unsigned char)(m_seq);
			++m_seq;

			memcpy(packet+12, p1, nalu_size);
			struct sockaddr_in addrin;
			socket_addr_ipv4(&addrin, m_ip.c_str(), m_port[0]);
			r = socket_sendto(m_socket[0], packet, nalu_size+12, 0, (struct sockaddr*)&addrin, sizeof(addrin));
			rtp_onsend(m_rtp, packet, nalu_size+12, clock);

			// single NAl unit packet 
			//packer->callback(packer->cbparam, p1, nalu_size);
		}
		else
		{
			// RFC6184 5.3. NAL Unit Header Usage: Table 2 (p15)
			// RFC6184 5.8. Fragmentation Units (FUs) (p29)
			unsigned char fu_indicator = (*p1 & 0xE0) | 28; // FU-A
			unsigned char fu_header = *p1 & 0x1F;

			// FU-A start
			fu_header = 0x80 | fu_header;
			while(nalu_size > MAX_UDP_PACKET)
			{
				packet[1] &= ~0x80; // clean marker
				packet[2] = (unsigned char)(m_seq >> 8);
				packet[3] = (unsigned char)(m_seq);
				packet[12] = fu_indicator;
				packet[13] = fu_header;
				++m_seq;

				memcpy(packet+14, p1, MAX_UDP_PACKET);
				struct sockaddr_in addrin;
				socket_addr_ipv4(&addrin, m_ip.c_str(), m_port[0]);
				r = socket_sendto(m_socket[0], packet, MAX_UDP_PACKET+14, 0, (struct sockaddr*)&addrin, sizeof(addrin));
				assert(r == MAX_UDP_PACKET+14 && 60 != r);
				rtp_onsend(m_rtp, packet, MAX_UDP_PACKET+14, clock);
				//packer->callback(packer->cbparam, fu_indicator, fu_header, p1, s_max_packet_size);

				nalu_size -= MAX_UDP_PACKET;
				p1 += MAX_UDP_PACKET;
				fu_header = 0x1F & fu_header; // FU-A fragment
			}

			// FU-A end
			fu_header = (0x40 | (fu_header & 0x1F));
			//packer->callback(packer->cbparam, fu_indicator, fu_header, p1, nalu_size);
			packet[1] |= 0x80; // marker
			packet[2] = (unsigned char)(m_seq >> 8);
			packet[3] = (unsigned char)(m_seq);
			packet[12] = fu_indicator;
			packet[13] = fu_header;
			++m_seq;

			while(nalu_size > 1 && 0 == p1[nalu_size-1])
				--nalu_size;

			memcpy(packet+14, p1, nalu_size);
			struct sockaddr_in addrin;
			socket_addr_ipv4(&addrin, m_ip.c_str(), m_port[0]);
			r = socket_sendto(m_socket[0], packet, nalu_size+14, 0, (struct sockaddr*)&addrin, sizeof(addrin));
			assert(r == nalu_size+14);
			rtp_onsend(m_rtp, packet, nalu_size+14, clock);
		}

		p1 = p2;
	}

	return 0;
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
