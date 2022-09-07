/*
C->S: 
DESCRIBE rtsp://server.example.com/fizzle/foo RTSP/1.0
CSeq: 312
Accept: application/sdp, application/rtsl, application/mheg

S->C: 
RTSP/1.0 200 OK
CSeq: 312
Date: 23 Jan 1997 15:35:06 GMT
Content-Type: application/sdp
Content-Length: 376

v=0
o=mhandley 2890844526 2890842807 IN IP4 126.16.64.4
s=SDP Seminar
i=A Seminar on the session description protocol
u=http://www.cs.ucl.ac.uk/staff/M.Handley/sdp.03.ps
e=mjh@isi.edu (Mark Handley)
c=IN IP4 224.2.17.12/127
t=2873397496 2873404696
a=recvonly
m=audio 3456 RTP/AVP 0
m=video 2232 RTP/AVP 31
m=whiteboard 32416 UDP WB
a=orient:portrait
*/

#include "rtsp-client-internal.h"

static const char* sc_format =
		"DESCRIBE %s RTSP/1.0\r\n"
		"CSeq: %u\r\n"
		"%s" // Authorization: Digest xxx
		"Accept: application/sdp\r\n"
		"User-Agent: %s\r\n"
		"\r\n";

int rtsp_client_describe(struct rtsp_client_t* rtsp)
{
	int r;
	rtsp->progress = 0;
	rtsp->state = RTSP_DESCRIBE;

	r = rtsp_client_authenrization(rtsp, "DESCRIBE", rtsp->uri, NULL, 0, rtsp->authenrization, sizeof(rtsp->authenrization));
	r = snprintf(rtsp->req, sizeof(rtsp->req), sc_format, rtsp->uri, rtsp->cseq++, rtsp->authenrization, USER_AGENT);
	assert(r > 0 && r < sizeof(rtsp->req));
	return r == rtsp->handler.send(rtsp->param, rtsp->uri, rtsp->req, r) ? 0 : -1;
}

int rtsp_client_describe_onreply(struct rtsp_client_t* rtsp, void* parser)
{
	int code, r;
	assert(0 == rtsp->progress);
	assert(RTSP_DESCRIBE == rtsp->state);

	r = -1;
	code = http_get_status_code(parser);
	if (200 == code)
	{
		const void* content;
		const char* contentType;
		const char* contentBase;
		const char* contentLocation;
		content = http_get_content(parser);
		contentType = http_get_header_by_name(parser, "Content-Type");
		contentBase = http_get_header_by_name(parser, "Content-Base");
		contentLocation = http_get_header_by_name(parser, "Content-Location");

		if ((contentBase && snprintf(rtsp->baseuri, sizeof(rtsp->baseuri), "%s", contentBase) >= sizeof(rtsp->baseuri))
			|| (contentLocation && snprintf(rtsp->location, sizeof(rtsp->location), "%s", contentLocation) >= sizeof(rtsp->location)))
			return -1;

		rtsp->auth_failed = 0;
		if (!contentType || 0 == strcasecmp("application/sdp", contentType))
		{
			r = rtsp->handler.ondescribe(rtsp->param, (const char*)content, (int)http_get_content_length(parser));
		}
	}
	else if (401 == code)
	{
		// Unauthorized
		const char* authenticate;
		authenticate = http_get_header_by_name(parser, "WWW-Authenticate");
		if (authenticate && 0 == rtsp->auth_failed++)
		{
			rtsp_client_www_authenticate(rtsp, authenticate);
			r = rtsp_client_describe(rtsp);
		}
	}

	return r;
}
