#include "rtsp-server-internal.h"
#include "rtsp-header-session.h"

void rtsp_server_teardown(struct rtsp_session_t *session, const char* uri)
{
	const char *psession;
	struct rtsp_header_session_t xsession;
	struct rtsp_server_t* ctx = session->server;

	psession = rtsp_get_header_by_name(session->parser, "Session");
	if (!psession || 0 != rtsp_header_session(psession, &xsession))
	{
		// 454 (Session Not Found)
		rtsp_server_reply(session, 454);
		return;
	}

	ctx->handler.teardown(ctx->param, session, uri, xsession.session);
}

void rtsp_server_reply_teardown(struct rtsp_session_t *session, int code)
{
	rtsp_server_reply(session, code);
}
