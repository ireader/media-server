#include "rtsp-client.h"
#include "client/rtsp-client-internal.h"
#include "rtsp-parser.h"
#include "sdp.h"
#include <stdlib.h>
#include <assert.h>
#include <time.h>

void* rtsp_client_create(const rtsp_client_t *client, void* transport, void* ptr)
{
	struct rtsp_client_context_t *ctx;
	ctx = (struct rtsp_client_context_t*)malloc(sizeof(*ctx) + 512);
	if(!ctx)
		return NULL;

	memset(ctx, 0, sizeof(*ctx));
	memcpy(&ctx->client, client, sizeof(ctx->client));
	ctx->transport = transport;
	ctx->param = ptr;

	srand((unsigned int)time(NULL));
	ctx->cseq = rand();
//	ctx->parser = rtsp_parser_create(RTSP_PARSER_CLIENT);

	ctx->status = RTSP_CREATE;
	return ctx;
}

int rtsp_client_destroy(void* rtsp)
{
	struct rtsp_client_context_t *ctx;
	ctx = (struct rtsp_client_context_t*)rtsp;

	rtsp_client_close(rtsp);

	if(ctx->media_ptr)
		free(ctx->media_ptr);
	return 0;
}

int rtsp_client_open(void* rtsp, const char* uri)
{
	int r;
	struct rtsp_client_context_t *ctx;
	ctx = (struct rtsp_client_context_t*)rtsp;

	strlcpy(ctx->uri, uri, sizeof(ctx->uri));
	r = rtsp_client_describe(ctx, uri);
//	r = rtsp_client_setup(ctx);
	return r;
}

int rtsp_client_open_with_sdp(void* rtsp, const char* uri, const char* sdp)
{
	int i, r;
	void *sdpparser; // sdp parser
	struct rtsp_media_t *media;
	struct rtsp_client_context_t *ctx;
	ctx = (struct rtsp_client_context_t*)rtsp;

	if(uri != ctx->uri)
		strlcpy(ctx->uri, uri, sizeof(ctx->uri));

	sdpparser = sdp_parse(sdp);
	if(!sdpparser)
		return -1;

	r = rtsp_client_sdp(ctx, sdpparser);
	sdp_destroy(sdpparser);
	if(0 != r)
		return r;

	for(i = 0; i < ctx->media_count; i++)
	{
		media = rtsp_get_media(ctx, i);
		r = ctx->client.rtpport(ctx->param, &media->transport.rtp.u.client_port1);
        if(0 != r)
			return r;

		if(0 == media->transport.rtp.u.client_port1)
		{
			media->transport.transport = RTSP_TRANSPORT_RTP_TCP;
			media->transport.rtp.u.client_port1 = 2*(unsigned short)i;
			media->transport.rtp.u.client_port2 = 2*(unsigned short)i + 1;
		}
		else
		{
			assert(0 == media->transport.rtp.u.client_port1 % 2);
			media->transport.transport = RTSP_TRANSPORT_RTP_UDP;
			media->transport.rtp.u.client_port2 = media->transport.rtp.u.client_port1 + 1;
		}
	}

	ctx->status = RTSP_SETUP;
	ctx->progress = 0;
	return rtsp_client_media_setup(ctx);
}

int rtsp_client_close(void* rtsp)
{
	struct rtsp_client_context_t *ctx;
	ctx = (struct rtsp_client_context_t*)rtsp;

	if(RTSP_TEARDWON == ctx->status)
		return 0;

	ctx->status = RTSP_TEARDWON;
	ctx->progress = 0;
	return rtsp_client_media_teardown(ctx);
}

int rtsp_media_count(void* rtsp)
{
	struct rtsp_client_context_t *ctx;
	ctx = (struct rtsp_client_context_t*)rtsp;
	return ctx->media_count;
}
