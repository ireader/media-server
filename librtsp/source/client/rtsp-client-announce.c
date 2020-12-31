/*
ANNOUNCE rtsp://server.example.com/fizzle/foo RTSP/1.0
CSeq: 312
Date: 23 Jan 1997 15:35:06 GMT
Session: 47112344
Content-Type: application/sdp
Content-Length: 332

v=0
o=mhandley 2890844526 2890845468 IN IP4 126.16.64.4
s=SDP Seminar
i=A Seminar on the session description protocol
u=http://www.cs.ucl.ac.uk/staff/M.Handley/sdp.03.ps
e=mjh@isi.edu (Mark Handley)
c=IN IP4 224.2.17.12/127
t=2873397496 2873404696
a=recvonly
m=audio 3456 RTP/AVP 0
m=video 2232 RTP/AVP 31
*/
#include "rtsp-client.h"
#include "rtsp-client-internal.h"

static const char* sc_format = 
		"ANNOUNCE %s RTSP/1.0\r\n"
		"CSeq: %u\r\n"
		"Session: %s\r\n"
		"%s" // Authorization: Digest xxx
		"Content-Type: application/sdp\r\n"
		"Content-Length: %u\r\n"
		"\r\n"
		"%s";

int rtsp_client_announce(struct rtsp_client_t* rtsp, const char* sdp)
{
	int r;
    rtsp->progress = 0;
	rtsp->state = RTSP_ANNOUNCE;
    rtsp->announce = sdp; // hijack

	r = rtsp_client_authenrization(rtsp, "ANNOUNCE", rtsp->uri, sdp, (int)strlen(sdp), rtsp->authenrization, sizeof(rtsp->authenrization));
	r = snprintf(rtsp->req, sizeof(rtsp->req), sc_format, rtsp->uri, rtsp->cseq++, rtsp->session[0].session, rtsp->authenrization, (unsigned int)strlen(sdp), sdp);
	assert(r > 0 && r < sizeof(rtsp->req));
	return r == rtsp->handler.send(rtsp->param, rtsp->uri, rtsp->req, r) ? 0 : -1;
}

int rtsp_client_announce_onreply(struct rtsp_client_t* rtsp, void* parser)
{
    int code, r;
    assert(0 == rtsp->progress);
    assert(RTSP_ANNOUNCE == rtsp->state);

    r = -1;
	code = http_get_status_code(parser);
	if (200 == code)
	{
        rtsp->auth_failed = 0;
        r = rtsp->handler.onannounce(rtsp->param);
	}
    else if(401 == code)
    {
        // Unauthorized
        const char* authenticate;
        authenticate = http_get_header_by_name(parser, "WWW-Authenticate");
        if (authenticate && 0 == rtsp->auth_failed++)
        {
            rtsp_client_www_authenticate(rtsp, authenticate);
            r = rtsp_client_announce(rtsp, rtsp->announce);
        }
    }

	return r;
}
