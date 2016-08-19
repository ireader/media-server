/*
C->S: 
TEARDOWN rtsp://example.com/fizzle/foo RTSP/1.0
CSeq: 892
Session: 12345678

S->C: 
RTSP/1.0 200 OK
CSeq: 892
*/

#include "rtsp-client-internal.h"

static void rtsp_client_media_teardown_onreply(void* rtsp, int r, void* parser)
{
	int code;
	struct rtsp_client_context_t* ctx;
	ctx = (struct rtsp_client_context_t*)rtsp;
	assert(RTSP_TEARDWON == ctx->status);
	assert(ctx->progress <= ctx->media_count);

	if(0 != r)
	{
		ctx->client.onclose(ctx->param, r);
		return;
	}

	code = rtsp_get_status_code(parser);
	if(200 == code)
	{
		if(ctx->media_count == ++ctx->progress)
		{
			ctx->client.onclose(ctx->param, 0);
		}
		else
		{
			r = rtsp_client_media_teardown(ctx);
			if(0 != r)
			{
				ctx->client.onclose(ctx->param, r);
			}
		}
	}
	else
	{
		ctx->client.onclose(ctx->param, -1);
	}
}

int rtsp_client_media_teardown(struct rtsp_client_context_t* ctx)
{
	int len;
	struct rtsp_media_t* media;

	assert(RTSP_TEARDWON == ctx->status);
	assert(ctx->progress < ctx->media_count);
	media = rtsp_get_media(ctx, ctx->progress);

	assert(media->uri[0] && media->session[0]);
	len = snprintf(ctx->req, sizeof(ctx->req),
		"TEARDOWN %s RTSP/1.0\r\n"
		"CSeq: %u\r\n"
		"Session: %s\r\n"
		"User-Agent: %s\r\n"
		"\r\n", 
		media->uri, media->cseq++, media->session, USER_AGENT);

	return ctx->client.request(ctx->transport, media->uri, ctx->req, len, ctx, rtsp_client_media_teardown_onreply);
}
