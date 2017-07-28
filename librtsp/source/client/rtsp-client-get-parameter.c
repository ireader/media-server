/*
S->C: 
GET_PARAMETER rtsp://example.com/fizzle/foo RTSP/1.0
CSeq: 431
Content-Type: text/parameters
Session: 12345678
Content-Length: 15
packets_received
jitter

C->S: 
RTSP/1.0 200 OK
CSeq: 431
Content-Length: 46
Content-Type: text/parameters
packets_received: 10
jitter: 0.3838
*/

#include "rtsp-client-internal.h"
#include <assert.h>

static const char* sc_format =
		"GET_PARAMETER %s RTSP/1.0\r\n"
		"CSeq: %u\r\n"
		"%s" // Session: %s\r\n
		"%s" // Authorization: Digest xxx
		"User-Agent: %s\r\n"
		"Content-Type: text/parameters\r\n"
		"Content-Length: %u\r\n"
		"\r\n"
		"%s";

int rtsp_client_get_parameter(struct rtsp_client_t *rtsp, int media, const char* parameter)
{
	int r;
	char session[128];
	struct rtsp_media_t* m;

	rtsp->state = RTSP_GET_PARAMETER;
	parameter = parameter ? parameter : "";

	m = rtsp_get_media(rtsp, media);
	if (m && m->uri[0] && m->session.session[0])
	{
		r = snprintf(session, sizeof(session), "Session: %s\r\n", m->session.session);
		if (r < 12 || r >= sizeof(session)) session[0] = '\0';

		r = rtsp_client_authenrization(rtsp, "GET_PARAMETER", m->uri, NULL, 0, rtsp->authenrization, sizeof(rtsp->authenrization));
		r = snprintf(rtsp->req, sizeof(rtsp->req), sc_format, m->uri, rtsp->cseq++, session, rtsp->authenrization, USER_AGENT, strlen(parameter), parameter);
		assert(r > 0 && r < sizeof(rtsp->req));
		return r = rtsp->handler.send(rtsp->param, m->uri, rtsp->req, r) ? 0 : -1;
	}
	else
	{
		r = rtsp_client_authenrization(rtsp, "GET_PARAMETER", rtsp->aggregate_uri[0] ? rtsp->aggregate_uri : rtsp->uri, NULL, 0, rtsp->authenrization, sizeof(rtsp->authenrization));
		r = snprintf(rtsp->req, sizeof(rtsp->req), sc_format, rtsp->aggregate_uri[0] ? rtsp->aggregate_uri : rtsp->uri, rtsp->cseq++, "", rtsp->authenrization, USER_AGENT, strlen(parameter), parameter);
		assert(r > 0 && r < sizeof(rtsp->req));
		return r == rtsp->handler.send(rtsp->param, rtsp->uri, rtsp->req, r) ? 0 : -1;
	}
}

int rtsp_client_get_parameter_onreply(struct rtsp_client_t* rtsp, void* parser)
{
	int code;
	const void* content;

	assert(RTSP_GET_PARAMETER == rtsp->state);
	code = rtsp_get_status_code(parser);
	if (200 == code)
	{
		content = rtsp_get_content(parser);
		return 0; // next step
	}
	else
	{
		return -1;
	}
}
