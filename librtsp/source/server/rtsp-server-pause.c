#include "rtsp-server-internal.h"
#include "rtsp-header-range.h"

// RFC2326 10.6 PAUSE (p36)
// 1. A PAUSE request discards all queued PLAY requests. However, the pause
//	  point in the media stream MUST be maintained. A subsequent PLAY
//	  request without Range header resumes from the pause point.
int rtsp_server_pause(struct rtsp_server_t* rtsp, const char* uri)
{
	int64_t npt = -1L;
	const char *prange;
	struct rtsp_header_range_t range;

	if (0 == rtsp->session.session[0])
	{
		// 454 Session Not Found
		return rtsp_server_reply(rtsp, 454);
	}

	prange = http_get_header_by_name(rtsp->parser, "range");
	if (prange && 0 == rtsp_header_range(prange, &range))
	{
		npt = range.from;
		// 10.6 The normal play time for the stream is set to the pause point. (p36)
		assert(range.type == RTSP_RANGE_NPT); // 3.6 Normal Play Time (p17)
		assert(range.to_value == RTSP_RANGE_TIME_NOVALUE);

		// 457 Invalid Range
		//rtsp_server_reply(req, 457, NULL);
		//return;
	}

	return rtsp->handler.onpause(rtsp->param, rtsp, uri, rtsp->session.session, -1L == npt ? NULL : &npt);
}

int rtsp_server_reply_pause(struct rtsp_server_t *rtsp, int code)
{
	return rtsp_server_reply(rtsp, code);
}
