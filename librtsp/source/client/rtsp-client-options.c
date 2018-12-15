/*
C->S: 
OPTIONS * RTSP/1.0
CSeq: 1
Require: implicit-play
Proxy-Require: gzipped-messages

S->C: 
RTSP/1.0 200 OK
CSeq: 1
Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE
*/

#include "rtsp-client-internal.h"

static const char* sc_format =
		"OPTIONS * RTSP/1.0\r\n"
		"CSeq: %u\r\n"
		"%s" // "Require: implicit-play\r\n"
		"%s" // "Session: xxx\r\n"
		"User-Agent: %s\r\n"
		"\r\n";

int rtsp_client_options(struct rtsp_client_t *rtsp, const char* commands)
{
	int r = 0;
	char require[128];
	char session[128];

	require[0] = '\0';
	session[0] = '\0';
	rtsp->state = RTSP_OPTIONS;

	if (commands && commands[0])
		snprintf(require, sizeof(require), "Require: %s\r\n", commands);
	if (rtsp->media_count > 0 && *rtsp->session[0].session)
		snprintf(session, sizeof(session), "Session: %s\r\n", rtsp->session[0].session);

	r = snprintf(rtsp->req, sizeof(rtsp->req), sc_format, rtsp->cseq++, require, session, USER_AGENT);
	assert(r > 0 && r < sizeof(rtsp->req));
	return r == rtsp->handler.send(rtsp->param, rtsp->uri, rtsp->req, r) ? 0 : -1;
}

int rtsp_client_options_onreply(struct rtsp_client_t* rtsp, void* parser)
{
	int code;
	const char *methods;

	assert(RTSP_OPTIONS == rtsp->state);
	code = http_get_status_code(parser);
	if (200 == code)
	{
		methods = http_get_header_by_name(parser, "Public");
		(void)methods; // TODO: check support methods
		return 0; // next step
	}
	else
	{
		return -1;
	}
}
