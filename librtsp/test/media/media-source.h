#ifndef _media_source_h_
#define _media_source_h_

#include <string>
#include <memory>

#ifndef MAX_UDP_PACKET
#define MAX_UDP_PACKET (1450-16)
#endif

struct IRTPTransport
{
	virtual int Send(bool rtcp, const void* data, size_t bytes) = 0;
};

struct IMediaSource
{
	virtual ~IMediaSource(){}

	virtual int Play() = 0;
	virtual int Pause() = 0;
	virtual int Seek(int64_t pos) = 0;
	virtual int SetSpeed(double speed) = 0;
	virtual int GetDuration(int64_t& duration) const = 0;
    virtual int GetSDPMedia(std::string& sdp) const = 0;
	virtual int GetRTPInfo(const char* uri, char *rtpinfo, size_t bytes) const = 0;
	virtual int SetTransport(const char* track, std::shared_ptr<IRTPTransport> transport) = 0;
};

#endif /* !_media_source_h_ */
