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
#include "rtsp-util.h"
#include <assert.h>

#define RTP_INFO_SPECIAL ",;\r\n"

int rtsp_header_rtp_info(const char* fields, struct rtsp_header_rtp_info_t* info)
{
	const char* p = fields;

	while(p && *p)
	{
		if(0 == strnicmp("url=", p, 4))
		{
			const char* p1 = NULL;
			p1 = string_token_word(p+4, RTP_INFO_SPECIAL);
			assert(p1 - p < sizeof(info->url)-1);
			strncpy(info->url, p+4, p1-p-4);
			info->url[p1-p-4] = '\0';
			p = p1;
		}
		else if(0 == strnicmp("seq=", p, 4))
		{
			p = string_token_int64(p+4, &info->seq);
			assert('\0' == p[0] || strchr(RTP_INFO_SPECIAL, p[0]));
		}
		else if(0 == strnicmp("rtptime=", p, 8))
		{
			p = string_token_int64(p+8, &info->rtptime);
			assert('\0' == p[0] || strchr(RTP_INFO_SPECIAL, p[0]));
		}
		else
		{
			p = string_token_word(p+1, RTP_INFO_SPECIAL);
		}

		if(!strchr(",\r\n", *p))
			++p;
	}

	return 0;
}

#if defined(DEBUG) || defined(_DEBUG)
void rtsp_header_rtp_info_test()
{
	struct rtsp_header_rtp_info_t rtp;

	memset(&rtp, 0, sizeof(rtp));
	assert(0 == rtsp_header_rtp_info("url=rtsp://foo.com/bar.avi/streamid=0;seq=45102", &rtp)); // rfc2326 p56
	assert(0 == strcmp(rtp.url, "rtsp://foo.com/bar.avi/streamid=0"));
	assert(rtp.seq == 45102);
}
#endif
