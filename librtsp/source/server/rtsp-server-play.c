#include "rtsp-server-internal.h"
#include "rtsp-header-session.h"
#include "rtsp-header-range.h"
#include "rfc822-datetime.h"

int rtsp_server_play(struct rtsp_server_t *rtsp, const char* uri)
{
	int64_t npt = -1L;
	double scale = 0.0f;
	const char *pscale, *prange, *psession;
	struct rtsp_header_range_t range;
	struct rtsp_header_session_t session;

	pscale = rtsp_get_header_by_name(rtsp->parser, "scale");
	prange = rtsp_get_header_by_name(rtsp->parser, "range");
	psession = rtsp_get_header_by_name(rtsp->parser, "Session");

	if (!psession || 0 != rtsp_header_session(psession, &session))
	{
		// 454 (Session Not Found)
		return rtsp_server_reply(rtsp, 454);
	}

	if (pscale)
	{
		scale = atof(pscale);
	}

	if (prange && 0 == rtsp_header_range(prange, &range))
	{
		npt = range.from;
	}

	return rtsp->handler.onplay(rtsp->param, rtsp, uri, session.session, -1L == npt ? NULL : &npt, pscale ? &scale : NULL);
}

int rtsp_server_reply_play(struct rtsp_server_t *rtsp, int code, const int64_t *nptstart, const int64_t *nptend, const char* rtp)
{
	int len;
	char range[64] = { 0 };
	char rtpinfo[256] = { 0 };
	rfc822_datetime_t datetime;

	if (200 != code)
		return rtsp_server_reply(rtsp, code);

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
	len = snprintf(rtsp->reply, sizeof(rtsp->reply),
		"RTSP/1.0 200 OK\r\n"
		"CSeq: %u\r\n"
		"Date: %s\r\n"
		"%s" // Range
		"%s" // RTP-Info
		"\r\n",
		rtsp->cseq, datetime, range, rtpinfo);

	return rtsp->handler.send(rtsp->sendparam, rtsp->reply, len);
}
