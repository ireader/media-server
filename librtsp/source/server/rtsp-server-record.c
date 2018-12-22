#include "rtsp-server-internal.h"
#include "rtsp-header-range.h"
#include "rfc822-datetime.h"

int rtsp_server_record(struct rtsp_server_t *rtsp, const char* uri)
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

    return rtsp->handler.onrecord(rtsp->param, rtsp, uri, rtsp->session.session, -1L == npt ? NULL : &npt, pscale ? &scale : NULL);
}

int rtsp_server_reply_record(struct rtsp_server_t *rtsp, int code, const int64_t *nptstart, const int64_t *nptend)
{
    int len = 0;
    char header[128] = { 0 };

    if (nptstart)
    {
        if (nptend)
            len += snprintf(header + len, sizeof(header) - len, "Range: npt=%.3f-%.3f\r\n", (float)(*nptstart / 1000.0f), (float)(*nptend / 1000.0f));
        else
            len += snprintf(header + len, sizeof(header) - len, "Range: npt=%.3f-\r\n", (float)(*nptstart / 1000.0f));
    }

    return rtsp_server_reply2(rtsp, code, header, NULL, 0);
}
