#ifndef _media_source_h_
#define _media_source_h_

#include "sys/sock.h"
#include "ctypedef.h"
#include <string>

struct IMediaSource
{
	virtual ~IMediaSource(){}

	virtual int Play() = 0;
	virtual int Pause() = 0;
	virtual int Seek(int64_t pos) = 0;
	virtual int SetSpeed(double speed) = 0;
	virtual int GetDuration(int64_t& duration) const = 0;
    virtual int GetSDPMedia(std::string& sdp) const = 0;
	virtual int GetRTPInfo(int64_t &pos, unsigned short &seq, unsigned int &rtptime) const = 0;
	virtual int SetRTPSocket(const char* ip, socket_t socket[2], unsigned short port[2]) = 0;
};

#endif /* !_media_source_h_ */
