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

static void rtsp_client_describe_onreply(void* rtsp, int r, void* parser)
{
	int code;
	const void* content;
	const char* contentType;
	const char* contentBase;
	const char* contentLocation;
	struct rtsp_client_context_t* ctx;

	ctx = (struct rtsp_client_context_t*)rtsp;
	assert(RTSP_DESCRIBE == ctx->status);
	assert(0 == ctx->progress);

	if(0 != r)
	{
		ctx->client.onopen(ctx->ptr, r, NULL, 0);
		return;
	}

	code = rtsp_get_status_code(parser);
	if(200 != code)
	{
		r = -1;
	}
	else
	{
		content = rtsp_get_content(parser);
		contentType = rtsp_get_header_by_name(parser, "Content-Type");
		contentBase = rtsp_get_header_by_name(parser, "Content-Base");
		contentLocation = rtsp_get_header_by_name(parser, "Content-Location");

		if(contentBase)
			strlcpy(ctx->baseuri, contentBase, sizeof(ctx->baseuri));
		if(contentLocation)
			strlcpy(ctx->location, contentLocation, sizeof(ctx->location));

		if(!contentType || 0 == strcasecmp("application/sdp", contentType))
			r = rtsp_client_open_with_sdp(rtsp, ctx->uri, content);
		else
			r = -1;
	}

	if(0 != r)
		ctx->client.onopen(ctx->ptr, r, NULL, 0);
}

int rtsp_client_describe(struct rtsp_client_context_t* ctx, const char* uri)
{
	ctx->status = RTSP_DESCRIBE;
	ctx->progress = 0;

	snprintf(ctx->req, sizeof(ctx->req), 
		"DESCRIBE %s RTSP/1.0\r\n"
		"CSeq: %u\r\n"
		"Accept: application/sdp\r\n"
		"User-Agent: %s\r\n"
		"\r\n", 
		uri, ctx->cseq++, USER_AGENT);

	return ctx->client.request(ctx->transport, ctx->uri, ctx->req, strlen(ctx->req), ctx, rtsp_client_describe_onreply);
}
