#include "rtsp-server-internal.h"
#include "rtsp-header-session.h"
#include "rtsp-header-range.h"

// RFC2326 10.6 PAUSE (p36)
// 1. A PAUSE request discards all queued PLAY requests. However, the pause
//	  point in the media stream MUST be maintained. A subsequent PLAY
//	  request without Range header resumes from the pause point.
void rtsp_server_pause(struct rtsp_session_t* session, const char* uri)
{
	int64_t npt = -1L;
	const char *prange, *psession;
	struct rtsp_header_range_t range;
	struct rtsp_header_session_t xsession;
	struct rtsp_server_t* ctx = session->server;

	prange = rtsp_get_header_by_name(session->parser, "range");
	psession = rtsp_get_header_by_name(session->parser, "Session");

	if (!psession || 0 != rtsp_header_session(psession, &xsession))
	{
		// 454 Session Not Found
		rtsp_server_reply(session, 454);
		return;
	}

	if (prange && 0 == rtsp_header_range(prange, &range))
	{
		npt = range.from;
		// 10.6 The normal play time for the stream is set to the pause point. (p36)
		assert(range.type == RTSP_RANGE_NPT); // 3.6 Normal Play Time (p17)
		assert(range.to_value == RTSP_RANGE_TIME_NOVALUE);

		// 457 Invalid Range
		//rtsp_server_reply(req, 457);
		//return;
	}

	ctx->handler.pause(ctx->param, session, uri, xsession.session, -1L == npt ? NULL : &npt);
}

void rtsp_server_reply_pause(struct rtsp_session_t *session, int code)
{
	rtsp_server_reply(session, code);
}
