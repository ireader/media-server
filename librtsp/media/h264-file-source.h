#ifndef _h264_file_source_h_
#define _h264_file_source_h_

#include "media-source.h"

class H264FileSource : public IMediaSource
{
public:
	static H264FileSource* Create();

protected:
	H264FileSource();
	virtual ~H264FileSource();

public:
	virtual int Play();
	virtual int Pause();
	virtual int Seek(unsigned int pos);
	virtual int GetDuration(unsigned int& duration);

	virtual int GetSDPMedia(std::string& sdp);
};

#endif /* !_h264_file_source_h_ */
