#include "h264-file-source.h"
#include "base64.h"
#include <assert.h>

H264FileSource::H264FileSource(const char *file)
:m_reader(file)
{
}

H264FileSource::~H264FileSource()
{
}

H264FileSource* H264FileSource::Create(const char *file)
{
	H264FileSource* s = new H264FileSource(file);
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

int H264FileSource::Seek(int64_t pos)
{
	return 0;
}

int H264FileSource::GetDuration(int64_t& duration)
{
	return 0;
}

int H264FileSource::GetSDPMedia(std::string& sdp)
{
    static const char* pattern =
        "m=video 0 RTP/AVP 98\n"
        "a=rtpmap:98 H264/90000\n"
        "a=fmtp:98 profile-level-id=%X%X%X;"
    			 "packetization-mode=1;"
    			 "sprop-parameter-sets=";

    char base64[512] = {0};
    std::string parameters;

    const std::list<H264FileReader::sps_t>& sps = m_reader.GetParameterSets();
    std::list<H264FileReader::sps_t>::const_iterator it;
    for(it = sps.begin(); it != sps.end(); ++it)
    {
        if(parameters.empty())
        {
            snprintf(base64, sizeof(base64), pattern, (unsigned int)(*it)[0], (unsigned int)(*it)[1], (unsigned int)(*it)[2]);
            sdp = base64;
        }
        else
        {
            parameters += ',';
        }

        size_t bytes = it->size();
        assert((bytes+2)/3*4 + bytes/57 + 1 < sizeof(base64));
        base64_encode(base64, &(*it)[0], bytes);
        assert(strlen(base64) > 0);
        parameters += base64;
    }

    sdp += parameters;
    sdp += '\n';
    return sps.empty() ? -1 : 0;
}
