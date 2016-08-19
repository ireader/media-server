#ifndef _h264_file_source_h_
#define _h264_file_source_h_

#include "h264-file-reader.h"
#include "media-source.h"
#include "sys/process.h"
#include "sys/sock.h"
#include "time64.h"
#include "rtp.h"
#include <string>

#ifndef MAX_UDP_PACKET
#define MAX_UDP_PACKET (1450-16)
#endif

class H264FileSource : public IMediaSource
{
public:
	H264FileSource(const char *file);
	virtual ~H264FileSource();

public:
	virtual int Play();
	virtual int Pause();
	virtual int Seek(int64_t pos);
	virtual int SetSpeed(double speed);
	virtual int GetDuration(int64_t& duration) const;
	virtual int GetSDPMedia(std::string& sdp) const;
	virtual int GetRTPInfo(int64_t &pos, unsigned short &seq, unsigned int &rtptime) const;
	virtual int SetRTPSocket(const char* ip, socket_t socket[2], unsigned short port[2]);

private:
	static void OnRTCPEvent(void* param, const struct rtcp_msg_t* msg);
	void OnRTCPEvent(const struct rtcp_msg_t* msg);
	int SendRTCP();

	static void* RTPAlloc(void* param, size_t bytes);
	static void RTPFree(void* param, void *packet);
	static void RTPPacket(void* param, void *packet, size_t bytes, uint64_t time);

private:
	void* m_rtp;
	time64_t m_rtp_clock;
	time64_t m_rtcp_clock;
    H264FileReader m_reader;
	socket_t m_socket[2];
	socklen_t m_addrlen[2];
	struct sockaddr_storage m_addr[2];

	int m_status;
	int64_t m_pos;
	double m_speed;

	void *m_rtppacker;
	unsigned char m_packet[MAX_UDP_PACKET+14];
};

#endif /* !_h264_file_source_h_ */
