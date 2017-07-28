#include "rtsp-server-internal.h"
#include "rtsp-header-session.h"
#include "rtsp-header-range.h"
#include "rfc822-datetime.h"

void rtsp_server_play(struct rtsp_session_t *session, const char* uri)
{
	int64_t npt = -1L;
	double scale = 0.0f;
	const char *pscale, *prange, *psession;
	struct rtsp_header_range_t range;
	struct rtsp_header_session_t xsession;
	struct rtsp_server_t* ctx = session->server;

	pscale = rtsp_get_header_by_name(session->parser, "scale");
	prange = rtsp_get_header_by_name(session->parser, "range");
	psession = rtsp_get_header_by_name(session->parser, "Session");

	if (!psession || 0 != rtsp_header_session(psession, &xsession))
	{
		// 454 (Session Not Found)
		rtsp_server_reply(session, 454);
		return;
	}

	if (pscale)
	{
		scale = atof(pscale);
	}

	if (prange && 0 == rtsp_header_range(prange, &range))
	{
		npt = range.from;
	}

	ctx->handler.play(ctx->param, session, uri, xsession.session, -1L == npt ? NULL : &npt, pscale ? &scale : NULL);
}

void rtsp_server_reply_play(struct rtsp_session_t *session, int code, const int64_t *nptstart, const int64_t *nptend, const char* rtp)
{
	int len;
	char range[64] = { 0 };
	char rtpinfo[256] = { 0 };
	rfc822_datetime_t datetime;

	if (200 != code)
	{
		rtsp_server_reply(session, code);
		return;
	}

	if (rtp)
	{
		len = snprintf(rtpinfo, sizeof(rtpinfo), "RTP-Info: %s\r\n", rtp);
	}

	if (nptstart)
	{
		if (nptend)
			len = snprintf(range, sizeof(range), "Range: %.3f-%.3f\r\n", (float)(*nptstart / 1000.0f), (float)(*nptend / 1000.0f));
		else
			len = snprintf(range, sizeof(range), "Range: %.3f-\r\n", (float)(*nptstart / 1000.0f));
	}

	rfc822_datetime_format(time(NULL), datetime);
	// smpte=0:10:22-;time=19970123T153600Z
	len = snprintf(session->reply, sizeof(session->reply),
		"RTSP/1.0 200 OK\r\n"
		"CSeq: %u\r\n"
		"Date: %s\r\n"
		"%s" // Range
		"%s" // RTP-Info
		"\r\n",
		session->cseq, datetime, range, rtpinfo);

	session->send(session, session->reply, len);
}
