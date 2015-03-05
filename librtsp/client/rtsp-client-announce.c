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

static void rtsp_client_onannounce(void* rtsp, int r, void* parser)
{
	int code;
	struct rtsp_client_context_t* ctx;

	ctx = (struct rtsp_client_context_t*)rtsp;
	//assert(0 == ctx->aggregate);
	//assert(RTSP_PAUSE == ctx->status);
	//assert(ctx->progress < ctx->media_count);

	if(0 != r)
	{
//		ctx->client.onopen(ctx->param, r);
		return;
	}

	code = rtsp_get_status_code(parser);
	if(200 == code)
	{
		//if(ctx->media_count == ++ctx->progress)
		//{
		//	ctx->client.onaction(ctx->param, 0);
		//}
		//else
		//{
		//	r = rtsp_client_media_pause(ctx);
		//	if(0 != r)
		//	{
		//		ctx->client.onaction(ctx->param, r);
		//	}
		//}
	}
	else
	{
//		ctx->client.onaction(ctx->param, -1);
	}
}

int rtsp_client_announce(struct rtsp_client_context_t* ctx, const char* sdp)
{
	assert(ctx->media_count > 0);
	snprintf(ctx->req, sizeof(ctx->req), 
		"ANNOUNCE %s RTSP/1.0\r\n"
		"CSeq: %u\r\n"
		"Session: %s\r\n"
		"Content-Type: application/sdp\r\n"
		"Content-Length: %u\r\n"
		"\r\n"
		"%s", 
		ctx->uri, ctx->cseq++, ctx->media[0].session, (unsigned int)strlen(sdp), sdp);

	return ctx->client.request(ctx->transport, ctx->uri, ctx->req, strlen(ctx->req), ctx, rtsp_client_onannounce);
}
