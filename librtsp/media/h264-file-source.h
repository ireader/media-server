#ifndef _h264_file_source_h_
#define _h264_file_source_h_

#include "h264-file-reader.h"
#include "media-source.h"
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
	virtual int GetDuration(int64_t& duration);
	virtual int GetSDPMedia(std::string& sdp);
    
private:
    H264FileReader m_reader;
};

#endif /* !_h264_file_source_h_ */
