#include "rtsp-server-internal.h"
#include "rtsp-header-session.h"

int rtsp_server_teardown(struct rtsp_server_t *rtsp, const char* uri)
{
	if (0 == rtsp->session.session[0])
	{
		// 454 (Session Not Found)
		return rtsp_server_reply(rtsp, 454);
	}

	return rtsp->handler.onteardown(rtsp->param, rtsp, uri, rtsp->session.session);
}

int rtsp_server_reply_teardown(struct rtsp_server_t *rtsp, int code)
{
	return rtsp_server_reply(rtsp, code);
}
