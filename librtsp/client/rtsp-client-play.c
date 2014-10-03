// RFC-2326 10.5 PLAY (p34)
// 1. A PLAY request without a Range header is legal. It starts playing a
//    stream from the beginning unless the stream has been paused. If a
//	  stream has been paused via PAUSE, stream delivery resumes at the pause point.
// 2. If a stream is playing, such a PLAY request causes no
//    further action and can be used by the client to test server liveness.

/*
PLAY rtsp://audio.example.com/audio RTSP/1.0
CSeq: 835
Session: 12345678
Range: npt=10-15

C->S: 
PLAY rtsp://audio.example.com/twister.en RTSP/1.0
CSeq: 833
Session: 12345678
Range: smpte=0:10:20-;time=19970123T153600Z

S->C: 
RTSP/1.0 200 OK
CSeq: 833
Date: 23 Jan 1997 15:35:06 GMT
Range: smpte=0:10:22-;time=19970123T153600Z

C->S: 
PLAY rtsp://audio.example.com/meeting.en RTSP/1.0
CSeq: 835
Session: 12345678
Range: clock=19961108T142300Z-19961108T143520Z

S->C: 
RTSP/1.0 200 OK
CSeq: 835
Date: 23 Jan 1997 15:35:06 GMT
*/

#include "rtsp-client.h"
#include "rtsp-client-internal.h"
#include <assert.h>

static void rtsp_client_media_play_onreply(void* rtsp, int r, void* parser)
{
	int code;
	struct rtsp_client_context_t* ctx;

	ctx = (struct rtsp_client_context_t*)rtsp;
	assert(RTSP_PLAY == ctx->status);
	assert(ctx->progress < ctx->media_count);

	if(0 != r)
	{
		ctx->client.onaction(ctx->param, r);
		return;
	}

	code = rtsp_get_status_code(parser);
	if(200 == code)
	{
		if(ctx->media_count == ++ctx->progress)
		{
			ctx->client.onaction(ctx->param, 0);
		}
		else
		{
			r = rtsp_client_media_play(ctx);
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

int rtsp_client_media_play(struct rtsp_client_context_t *ctx)
{
	int i, r=0;
	struct rtsp_media_t* media;

	assert(0 == ctx->aggregate);
	assert(RTSP_PLAY == ctx->status);
	assert(ctx->progress < ctx->media_count);

	media = rtsp_get_media(ctx, ctx->progress);
	assert(media && media->uri && media->session);
	snprintf(ctx->req, sizeof(ctx->req), 
		"PLAY %s RTSP/1.0\r\n"
		"CSeq: %u\r\n"
		"Session: %s\r\n"
		"%s" // Range
		"%s" // Speed
		"User-Agent: %s\r\n"
		"\r\n", 
		media->uri, media->cseq++, media->session, ctx->range, ctx->speed, USER_AGENT);

	return ctx->client.request(ctx->param, media->uri, ctx->req, strlen(ctx->req), ctx, rtsp_client_media_play_onreply);
}

// aggregate control reply
static void rtsp_client_aggregate_play_onreply(void* rtsp, int r, void* parser)
{
	int code;
	struct rtsp_client_context_t* ctx;

	ctx = (struct rtsp_client_context_t*)rtsp;
	assert(RTSP_PLAY == ctx->status);
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
		r = rtsp_client_media_play(ctx);
		if(0 != r)
			ctx->client.onaction(ctx->param, -1);
	}
	else
	{
		ctx->client.onaction(ctx->param, 200==code ? 0 : -1);
	}
}

int rtsp_client_play(void* rtsp, const int64_t *npt, const float *speed)
{
	struct rtsp_client_context_t *ctx;
	ctx = (struct rtsp_client_context_t*)rtsp;
	if(RTSP_SETUP != ctx->status)
		return -1;

	ctx->status = RTSP_PLAY;
	ctx->progress = 0;

	if(npt)
		snprintf(ctx->range, sizeof(ctx->range), "Range: npt=%lld.%d-\r\n", *npt/1000, *npt%1000);
	else
		ctx->range[0] = '\0';

	if(speed)
		snprintf(ctx->speed, sizeof(ctx->speed), "Speed: %f\r\n", *speed);
	else
		ctx->speed[0] = '\0';

	if(ctx->aggregate)
	{
		assert(ctx->session[0] && ctx->aggregate_uri[0]);
		snprintf(ctx->req, sizeof(ctx->req), 
			"PLAY %s RTSP/1.0\r\n"
			"CSeq: %u\r\n"
			"Session: %s\r\n"
			"%s" // Range
			"%s" // Speed
			"User-Agent: %s\r\n"
			"\r\n",
			ctx->aggregate_uri, ctx->cseq++, ctx->session, ctx->range, ctx->speed, USER_AGENT);

		return ctx->client.request(ctx->param, ctx->aggregate_uri, ctx->req, strlen(ctx->req), ctx, rtsp_client_aggregate_play_onreply);
	}
	else
	{
		return rtsp_client_media_play(ctx);
	}
}
