// rfc2326 12.33 RTP-Info (p55)
// used to set RTP-specific parameters in the PLAY response
// parameter: url rtp stream url
// parameter: seq digit
// parameter: rtptime digit
// RTP-Info = "RTP-Info" ":" 1#stream-url 1*parameter
// stream-url = "url" "=" url
// parameter = ";" "seq" "=" 1*DIGIT
//			| ";" "rtptime" "=" 1*DIGIT
//
// e.g. RTP-Info: url=rtsp://foo.com/bar.avi/streamid=0;seq=45102,url=rtsp://foo.com/bar.avi/streamid=1;seq=30211

#include "rtsp-header-rtp-info.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#if defined(_WIN32) || defined(_WIN64) || defined(OS_WINDOWS)
#define strncasecmp	_strnicmp
#endif

#define RTP_INFO_SPECIAL ",;\r\n"

int rtsp_header_rtp_info(const char* field, struct rtsp_header_rtp_info_t* rtpinfo)
{
	const char* p1;
	const char* p = field;

	while(p && *p)
	{
		p1 = strpbrk(p, RTP_INFO_SPECIAL);
		if(0 == strncasecmp("url=", p, 4))
		{
            size_t n = (size_t)(p1 - p - 4); // ptrdiff_t -> size_t
			if(n >= sizeof(rtpinfo->url))
                return -1;
			memcpy(rtpinfo->url, p+4, n);
			rtpinfo->url[n] = '\0';
		}
		else if(1 == sscanf(p, "seq = %" PRIu64, &rtpinfo->seq))
		{
		}
		else if(1 == sscanf(p, "rtptime = %" PRIu64, &rtpinfo->rtptime))
		{
		}
		else
		{
			assert(0); // unknown parameter
		}

		if(NULL == p1 || '\r' == *p1 || '\n' == *p1 || '\0' == *p1 || ',' == *p1)
			break;
		p = p1 + 1;
	}

	return 0;
}

#if defined(DEBUG) || defined(_DEBUG)
void rtsp_header_rtp_info_test(void)
{
	struct rtsp_header_rtp_info_t rtp;

	memset(&rtp, 0, sizeof(rtp));
	assert(0 == rtsp_header_rtp_info("url=rtsp://foo.com/bar.avi/streamid=0;seq=45102", &rtp)); // rfc2326 p56
	assert(0 == strcmp(rtp.url, "rtsp://foo.com/bar.avi/streamid=0"));
	assert(rtp.seq == 45102);

	memset(&rtp, 0, sizeof(rtp));
	assert(0 == rtsp_header_rtp_info("url=rtsp://foo.com/bar.avi/streamid=0;seq=45102;rtptime=123456789, url=rtsp://foo.com/bar.avi/streamid=1;seq=30211", &rtp));
	assert(0 == strcmp(rtp.url, "rtsp://foo.com/bar.avi/streamid=0"));
	assert(rtp.seq == 45102 && rtp.rtptime == 123456789);
}
#endif
