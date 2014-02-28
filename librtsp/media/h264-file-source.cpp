#include "h264-file-source.h

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
