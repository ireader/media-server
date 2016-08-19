#ifndef _ps_file_source_h_
#define _ps_file_source_h_

#include "h264-file-reader.h"
#include "media-source.h"
#include "sys/process.h"
#include "time64.h"
#include "rtp.h"
#include <string>

#ifndef MAX_UDP_PACKET
#define MAX_UDP_PACKET (1450-16)
#endif

class PSFileSource : public IMediaSource
{
public:
	PSFileSource(const char *file);
	virtual ~PSFileSource();

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
	static void *Alloc(void* param, size_t bytes);
	static void Free(void* param, void* packet);
	static void Packet(void* param, int avtype, void* packet, size_t bytes);
	static void* RTPAlloc(void* param, size_t bytes);
	static void RTPFree(void* param, void *packet);
	static void RTPPacket(void* param, void *packet, size_t bytes, uint64_t time);

	unsigned char* CreateRTPPacket();

	static void OnRTCPEvent(void* param, const struct rtcp_msg_t* msg);
	void OnRTCPEvent(const struct rtcp_msg_t* msg);
	int SendRTCP();

private:
	void* m_ps;
	void* m_rtp;
	int m_status;
	int64_t m_pos;
	double m_speed;
	unsigned short m_seq;

	time64_t m_ps_clock;
	time64_t m_rtp_clock;
	time64_t m_rtcp_clock;
	H264FileReader m_reader;
	socket_t m_socket[2];
	socklen_t m_addrlen[2];
	struct sockaddr_storage m_addr[2];

	void *m_pspacker;
	unsigned char m_packet[MAX_UDP_PACKET+14];
};

#endif /* !_ps_file_source_h_ */
