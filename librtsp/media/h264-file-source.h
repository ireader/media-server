#ifndef _h264_file_source_h_
#define _h264_file_source_h_

#include "h264-file-reader.h"
#include "media-source.h"
#include "sys/process.h"
#include "time64.h"
#include "rtp.h"
#include <string>

class H264FileSource : public IMediaSource
{
public:
	static H264FileSource* Create(const char* file);

protected:
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
	int Pack(const void* h264, size_t bytes);

private:
	void* m_rtp;
	int m_status;
	unsigned int m_ssrc;
	unsigned int m_timestamp;
	unsigned short m_seq;
	time64_t m_rtp_clock;
	time64_t m_rtcp_clock;
    H264FileReader m_reader;
	socket_t m_socket[2];
	unsigned short m_port[2];
	std::string m_ip;
};

#endif /* !_h264_file_source_h_ */
