#include "h264-file-source.h"

H264FileSource::H264FileSource()
{
}

H264FileSource::~H264FileSource()
{
}

H264FileSource* H264FileSource::Create()
{
	H264FileSource* s = new H264FileSource();
	return s;
}

int H264FileSource::Play()
{
	return 0;
}

int H264FileSource::Pause()
{
	return 0;
}

int H264FileSource::Seek(unsigned int pos)
{
	return 0;
}

int H264FileSource::GetDuration(unsigned int& duration)
{
	return 0;
}

int H264FileSource::GetSDPMedia(std::string& sdp)
{
	return 0;
}
