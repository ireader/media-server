#ifndef _media_source_h_
#define _media_source_h_

#include <string>

#if defined(OS_WINDOWS)
typedef __int64				int64_t;
#else
typedef long long			int64_t;
#endif

struct IMediaSource
{
	virtual ~IMediaSource(){}

	virtual int Play() = 0;
	virtual int Pause() = 0;
	virtual int Seek(int64_t pos) = 0;
	virtual int GetDuration(int64_t& duration) = 0;
    virtual int GetSDPMedia(std::string& sdp) = 0;
};

#endif /* !_media_source_h_ */
