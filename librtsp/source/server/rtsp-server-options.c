#include "rtsp-server-internal.h"

// RFC 2326 10.1 OPTIONS (p30)
int rtsp_server_options(struct rtsp_server_t* rtsp, const char* uri)
{
	http_get_header_by_name(rtsp->parser, "Connection");
	http_get_header_by_name(rtsp->parser, "Require");
	http_get_header_by_name(rtsp->parser, "Proxy-Require");
	http_get_header_by_name(rtsp->parser, "Proxy-Authenticate");

	if (rtsp->handler.onoptions)
		return rtsp->handler.onoptions(rtsp->param, rtsp, uri);
	else
		return rtsp_server_reply_options(rtsp, 200);
}

int rtsp_server_reply_options(rtsp_server_t* rtsp, int code)
{
	return rtsp_server_reply2(rtsp, code, "Public: DESCRIBE,SETUP,TEARDOWN,PLAY,PAUSE,ANNOUNCE,RECORD,GET_PARAMETER,SET_PARAMETER\r\n", NULL, 0);
}
