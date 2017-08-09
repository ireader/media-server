#include "rtsp-server-internal.h"
#include "rfc822-datetime.h"

int rtsp_server_describe(struct rtsp_server_t *rtsp, const char* uri)
{
	return rtsp->handler.ondescribe(rtsp->param, rtsp, uri);
}

int rtsp_server_reply_describe(struct rtsp_server_t *rtsp, int code, const char* sdp)
{
	int len;
	rfc822_datetime_t datetime;

	if (200 != code)
		return rtsp_server_reply(rtsp, code);

	len = snprintf(rtsp->reply, sizeof(rtsp->reply),
		"RTSP/1.0 200 OK\r\n"
		"CSeq: %u\r\n"
		"Date: %s\r\n"
		"Content-Type: application/sdp\r\n"
		"Content-Length: %u\r\n"
		"\r\n"
		"%s",
		rtsp->cseq,
		rfc822_datetime_format(time(NULL), datetime),
		(unsigned int)strlen(sdp), sdp);

	return rtsp->handler.send(rtsp->sendparam, rtsp->reply, len);
}
