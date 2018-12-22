#include "rtsp-server-internal.h"
#include "rtsp-header-range.h"
#include "rfc822-datetime.h"

int rtsp_server_play(struct rtsp_server_t *rtsp, const char* uri)
{
	int64_t npt = -1L;
	double scale = 0.0f;
	const char *pscale, *prange;
	struct rtsp_header_range_t range;

	pscale = http_get_header_by_name(rtsp->parser, "scale");
	prange = http_get_header_by_name(rtsp->parser, "range");

	if (0 == rtsp->session.session[0])
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

	return rtsp->handler.onplay(rtsp->param, rtsp, uri, rtsp->session.session, -1L == npt ? NULL : &npt, pscale ? &scale : NULL);
}

int rtsp_server_reply_play(struct rtsp_server_t *rtsp, int code, const int64_t *nptstart, const int64_t *nptend, const char* rtp)
{
	int n = 0;
	char header[1024] = { 0 };

	if (n >= 0 && nptstart)
	{
		if (nptend)
			n += snprintf(header + n, sizeof(header) - n, "Range: npt=%.3f-%.3f\r\n", (float)(*nptstart / 1000.0f), (float)(*nptend / 1000.0f));
		else
			n += snprintf(header + n, sizeof(header) - n, "Range: npt=%.3f-\r\n", (float)(*nptstart / 1000.0f));
	}

	if (n >= 0 && rtp)
	{
		n += snprintf(header + n, sizeof(header) - n, "RTP-Info: %s\r\n", rtp);
	}

	if (n < 0 || n >= sizeof(header))
	{
		assert(0); // rtp-info too long
		return -1;
	}

	return rtsp_server_reply2(rtsp, code, header, NULL, 0);
}
