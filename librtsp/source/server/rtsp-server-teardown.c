#include "rtsp-server-internal.h"
#include "rtsp-header-session.h"

int rtsp_server_teardown(struct rtsp_server_t *rtsp, const char* uri)
{
	const char *psession;
	struct rtsp_header_session_t session;

	psession = http_get_header_by_name(rtsp->parser, "Session");
	if (!psession || 0 != rtsp_header_session(psession, &session))
	{
		// 454 (Session Not Found)
		return rtsp_server_reply(rtsp, 454);
	}

	return rtsp->handler.onteardown(rtsp->param, rtsp, uri, session.session);
}

int rtsp_server_reply_teardown(struct rtsp_server_t *rtsp, int code)
{
	return rtsp_server_reply(rtsp, code);
}
