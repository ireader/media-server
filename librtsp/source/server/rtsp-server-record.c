#include "rtsp-server-internal.h"
#include "rtsp-header-range.h"
#include "rfc822-datetime.h"

int rtsp_server_record(struct rtsp_server_t *rtsp, const char* uri)
{
    int64_t npt = -1L;
    double scale = 0.0f;
    const char *pscale, *prange, *psession;
    struct rtsp_header_range_t range;

    pscale = http_get_header_by_name(rtsp->parser, "scale");
    prange = http_get_header_by_name(rtsp->parser, "range");
    psession = http_get_header_by_name(rtsp->parser, "Session");

    rtsp->session.session[0] = 0; // clear session value
    if (!psession || 0 != rtsp_header_session(psession, &rtsp->session))
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

    return rtsp->handler.onrecord(rtsp->param, rtsp, uri, rtsp->session.session, -1L == npt ? NULL : &npt, pscale ? &scale : NULL);
}

int rtsp_server_reply_record(struct rtsp_server_t *rtsp, int code, const int64_t *nptstart, const int64_t *nptend)
{
    int len = 0;
    char header[1024] = { 0 };

    if (nptstart)
    {
        if (nptend)
            len += snprintf(header + len, sizeof(header) - len, "Range: npt=%.3f-%.3f\r\n", (float)(*nptstart / 1000.0f), (float)(*nptend / 1000.0f));
        else
            len += snprintf(header + len, sizeof(header) - len, "Range: npt=%.3f-\r\n", (float)(*nptstart / 1000.0f));
    }

    if (rtsp->session.session[0])
    {
        len += snprintf(header + len, sizeof(header) - len, "Session: %s\r\n", rtsp->session.session);
    }

    return rtsp_server_reply2(rtsp, code, header);
}
