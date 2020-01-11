#ifndef _ps_file_source_h_
#define _ps_file_source_h_

#include "h264-file-reader.h"
#include "media-source.h"
#include "mpeg-ps.h"
#include "sys/process.h"
#include "time64.h"
#include "rtp.h"
#include <string>

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
	virtual int GetRTPInfo(const char* uri, char *rtpinfo, size_t bytes) const;
	virtual int SetTransport(const char* track, std::shared_ptr<IRTPTransport> transport);

private:
	static void *Alloc(void* param, size_t bytes);
	static void Free(void* param, void* packet);
	static void Packet(void* param, int avtype, void* packet, size_t bytes);
	static void* RTPAlloc(void* param, int bytes);
	static void RTPFree(void* param, void *packet);
	static void RTPPacket(void* param, const void *packet, int bytes, uint32_t timestamp, int flags);

	unsigned char* CreateRTPPacket();

	static void OnRTCPEvent(void* param, const struct rtcp_msg_t* msg);
	void OnRTCPEvent(const struct rtcp_msg_t* msg);
	int SendRTCP();

private:
	ps_muxer_t* m_ps;
    int m_ps_stream;
	void* m_rtp;
	int m_status;
	int64_t m_pos;
	double m_speed;
	unsigned short m_seq;

	time64_t m_ps_clock;
	time64_t m_rtp_clock;
	time64_t m_rtcp_clock;
	H264FileReader m_reader;
	std::shared_ptr<IRTPTransport> m_transport;

	void *m_pspacker;
	unsigned char m_packet[MAX_UDP_PACKET+14];
};

#endif /* !_ps_file_source_h_ */
