#include "rtsp-server-internal.h"
#include "http-header-content-type.h"
#include "rfc822-datetime.h"

int rtsp_server_announce(struct rtsp_server_t *rtsp, const char* uri)
{
    const char* content;
    const char* pcontenttype;
    struct http_header_content_type_t content_type;
    memset(&content_type, 0, sizeof(content_type));
    pcontenttype = http_get_header_by_name(rtsp->parser, "Content-Type");
    if (!pcontenttype || 0 != http_header_content_type(pcontenttype, &content_type) 
        || 0 != strcasecmp(content_type.media_type, "application") 
        || 0 != strcasecmp(content_type.media_subtype, "sdp"))
    {
        // 406 Not Acceptable
        // 415 Unsupported Media Type ?
        return rtsp_server_reply(rtsp, 406);
    }

    content = (const char*)http_get_content(rtsp->parser);
    return rtsp->handler.onannounce(rtsp->param, rtsp, uri, content);
}

int rtsp_server_reply_announce(struct rtsp_server_t *rtsp, int code)
{
    int len;
    rfc822_datetime_t datetime;

    if (200 != code)
        return rtsp_server_reply(rtsp, code);

    len = snprintf(rtsp->reply, sizeof(rtsp->reply),
        "RTSP/1.0 200 OK\r\n"
        "CSeq: %u\r\n"
        "Date: %s\r\n"
        "\r\n",
        rtsp->cseq,
        rfc822_datetime_format(time(NULL), datetime));

    return rtsp->handler.send(rtsp->sendparam, rtsp->reply, len);
}
