#ifndef _rtsp_header_rtp_info_h_
#define _rtsp_header_rtp_info_h_

#include "ctypedef.h"

struct rtsp_header_rtp_info_t
{
	char url[256];
	int64_t seq;
	int64_t rtptime;
};

#endif /* !_rtsp_header_rtp_info_h_ */
