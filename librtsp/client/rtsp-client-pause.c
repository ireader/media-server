// RFC-2326 10.6 PAUSE (p36)
// 1. A PAUSE request discards all queued PLAY requests. However, the pause
//    point in the media stream MUST be maintained. A subsequent PLAY
//    request without Range header resumes from the pause point. (p36) 
// 2. The PAUSE request may contain a Range header specifying when the
//    stream or presentation is to be halted. (p36) (p45 for more)

/*
C->S: 
PAUSE rtsp://example.com/fizzle/foo RTSP/1.0
CSeq: 834
Session: 12345678

S->C: 
RTSP/1.0 200 OK
CSeq: 834
Date: 23 Jan 1997 15:35:06 GMT
*/

#include "rtsp-client.h"
#include "rtsp-client-internal.h"

static void rtsp_client_media_pause_onreply(void* rtsp, int r, void* parser)
{
	int code;
	struct rtsp_client_context_t* ctx;

	ctx = (struct rtsp_client_context_t*)rtsp;
	assert(0 == ctx->aggregate);
	assert(RTSP_PAUSE == ctx->status);
	assert(ctx->progress < ctx->media_count);

	if(0 != r)
	{
		ctx->client.onaction(ctx->param, r);
		return;
	}

	code = rtsp_get_status_code(parser);
	assert(460 != code); // 460 Only aggregate operation allowed (p26)

	if(200 == code)
	{
		if(ctx->media_count == ++ctx->progress)
		{
			ctx->client.onaction(ctx->param, 0);
		}
		else
		{
			r = rtsp_client_media_pause(ctx);
			if(0 != r)
			{
				ctx->client.onaction(ctx->param, r);
			}
		}
	}
	else
	{
		ctx->client.onaction(ctx->param, -1);
	}
}

int rtsp_client_media_pause(struct rtsp_client_context_t *ctx)
{
	int i, r=0;
	struct rtsp_media_t* media;

	assert(0 == ctx->aggregate);
	assert(RTSP_PAUSE == ctx->status);
	assert(ctx->progress < ctx->media_count);

	media = rtsp_get_media(ctx, ctx->progress);
	assert(media && media->uri && media->session);
	snprintf(ctx->req, sizeof(ctx->req), 
		"PAUSE %s RTSP/1.0\r\n"
		"CSeq: %u\r\n"
		"Session: %s\r\n"
		"User-Agent: %s\r\n"
		"\r\n", 
		media->uri, media->cseq++, media->session, USER_AGENT);

	return ctx->client.request(ctx->param, media->uri, ctx->req, strlen(ctx->req), ctx, rtsp_client_media_pause_onreply);
}

// aggregate control reply
static void rtsp_client_aggregate_pause_onreply(void* rtsp, int r, void* parser)
{
	int code;
	struct rtsp_client_context_t* ctx;

	ctx = (struct rtsp_client_context_t*)rtsp;
	assert(RTSP_PAUSE == ctx->status);
	assert(0 == ctx->progress);
	assert(ctx->aggregate);
	if(0 != r)
	{
		ctx->client.onaction(ctx->param, r);
		return;
	}

	code = rtsp_get_status_code(parser);
	if(459 == code) // 459 Aggregate operation not allowed (p26)
	{
		r = rtsp_client_media_pause(ctx);
		if(0 != r)
			ctx->client.onaction(ctx->param, -1);
	}
	else
	{
		ctx->client.onaction(ctx->param, 200==code ? 0 : -1);
	}
}

int rtsp_client_pause(void* rtsp)
{
	struct rtsp_client_context_t *ctx;
	ctx = (struct rtsp_client_context_t*)rtsp;
	if(RTSP_SETUP != ctx->status)
		return -1;

	ctx->status = RTSP_PAUSE;
	ctx->progress = 0;

	if(ctx->aggregate)
	{
		assert(ctx->session[0] && ctx->aggregate_uri[0]);
		snprintf(ctx->req, sizeof(ctx->req), 
			"PAUSE %s RTSP/1.0\r\n"
			"CSeq: %u\r\n"
			"Session: %s\r\n"
			"User-Agent: %s\r\n"
			"\r\n", 
			ctx->aggregate_uri, ctx->cseq++, ctx->session, USER_AGENT);

		return ctx->client.request(ctx->param, ctx->aggregate_uri, ctx->req, strlen(ctx->req), ctx, rtsp_client_aggregate_pause_onreply);
	}
	else
	{
		return rtsp_client_media_pause(ctx);
	}
}
