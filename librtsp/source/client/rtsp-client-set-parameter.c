/*
C->S: 
SET_PARAMETER rtsp://example.com/fizzle/foo RTSP/1.0
CSeq: 421
Content-length: 20
Content-type: text/parameters
barparam: barstuff

S->C: 
RTSP/1.0 451 Invalid Parameter
CSeq: 421
Content-length: 10
Content-type: text/parameters
barparam
*/

#include "rtsp-client-internal.h"
#include <assert.h>

static const char* sc_format =
		"SET_PARAMETER %s RTSP/1.0\r\n"
		"CSeq: %u\r\n"
		"%s" // Session: %s\r\n
		"%s" // Authorization: Digest xxx
		"User-Agent: %s\r\n"
		"Content-Type: text/parameters\r\n"
		"Content-Length: %u\r\n"
		"\r\n"
		"%s";

int rtsp_client_set_parameter(struct rtsp_client_t *rtsp, int media, const char* parameter)
{
	int r;
	char session[128];

	rtsp->state = RTSP_SET_PARAMETER;
	parameter = parameter ? parameter : "";
	if (media >= rtsp->media_count) return -1;

	if (media < rtsp->media_count && rtsp->media[media].uri[0] && rtsp->session[media].session[0])
	{
		r = snprintf(session, sizeof(session), "Session: %s\r\n", rtsp->session[media].session);
		if (r < 12 || r >= sizeof(session)) session[0] = '\0';

		r = rtsp_client_authenrization(rtsp, "SET_PARAMETER", rtsp->media[media].uri, NULL, 0, rtsp->authenrization, sizeof(rtsp->authenrization));
		r = snprintf(rtsp->req, sizeof(rtsp->req), sc_format, rtsp->media[media].uri, rtsp->cseq++, session, rtsp->authenrization, USER_AGENT, strlen(parameter), parameter);
		return (r > 0 && r < sizeof(rtsp->req) && r == rtsp->handler.send(rtsp->param, rtsp->media[media].uri, rtsp->req, r)) ? 0 : -1;
	}
	else
	{
		r = rtsp_client_authenrization(rtsp, "SET_PARAMETER", rtsp->aggregate_uri[0] ? rtsp->aggregate_uri : rtsp->uri, NULL, 0, rtsp->authenrization, sizeof(rtsp->authenrization));
		r = snprintf(rtsp->req, sizeof(rtsp->req), sc_format, rtsp->aggregate_uri[0] ? rtsp->aggregate_uri : rtsp->uri, rtsp->cseq++, "", rtsp->authenrization, USER_AGENT, strlen(parameter), parameter);
		return (r > 0 && r < sizeof(rtsp->req) && r == rtsp->handler.send(rtsp->param, rtsp->uri, rtsp->req, r)) ? 0 : -1;
	}
}

int rtsp_client_set_parameter_onreply(struct rtsp_client_t* rtsp, void* parser)
{
	int code;
	const void* content;

	assert(RTSP_SET_PARAMETER == rtsp->state);
	code = http_get_status_code(parser);
	if (200 == code)
	{
		content = http_get_content(parser);
		(void)content; // TODO: callback(content)
		return 0; // next step
	}
	else
	{
		return -1;
	}
}

