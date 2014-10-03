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

int rtsp_client_announce(struct rtsp_client_context_t* ctx, const char* sdp)
{
	socket_bufvec_t vec[2];

	snprintf(ctx->req, sizeof(ctx->req), 
		"ANNOUNCE %s RTSP/1.0\r\n"
		"CSeq: %u\r\n"
		"Session: %s\r\n"
		"Content-Type: application/sdp\r\n"
		"Content-Length: %d\r\n"
		"\r\n", 
		ctx->uri, ctx->cseq++, ctx->session, strlen(sdp));

	socket_setbufvec(vec, 0, ctx->req, strlen(ctx->req));
	socket_setbufvec(vec, 1, (void*)sdp, strlen(sdp));
	return rtsp_request_v(ctx, vec, 2);
}
