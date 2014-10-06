#include "rtsp-client.h"
#include "rtsp-client-internal.h"
#include "rtsp-parser.h"
#include "cstringext.h"
#include "sys/sock.h"
#include "aio-socket.h"
#include "url.h"
#include "sdp.h"
#include "rtsp-header-range.h"
#include "rtsp-header-rtp-info.h"
#include "rtsp-header-transport.h"
#include <stdlib.h>
#include <assert.h>
#include <time.h>

static int rtsp_create_rtp_socket(socket_t *rtp, socket_t *rtcp, int *port)
{
	unsigned short i;
	socket_t sock[2];
	assert(0 == RTP_PORT_BASE % 2);
	srand((unsigned int)time(NULL));

	do
	{
		i = rand() % 30000;
		i = i/2*2 + RTP_PORT_BASE;

		sock[0] = rtp_udp_socket(i);
		if(socket_invalid == sock[0])
			continue;

		sock[1] = rtp_udp_socket(i + 1);
		if(socket_invalid == sock[1])
		{
			socket_close(sock[0]);
			continue;
		}

		*rtp = sock[0];
		*rtcp = sock[1];
		*port = i;
		return 0;

	} while(socket_invalid!=sock[0] && socket_invalid!=sock[1]);

	return -1;
}

void* rtsp_client_create(const rtsp_client_transport_t *transport, void* ptr)
{
	int r;
	struct rtsp_client_context_t *ctx;
	ctx = (struct rtsp_client_context_t*)malloc(sizeof(ctx[0]) + 512);
	if(!ctx)
		return NULL;

	memset(ctx, 0, sizeof(struct rtsp_client_context_t));

	memcpy(&ctx->transport, transport, sizeof(ctx->transport));
	ctx->param = ptr;

	srand((unsigned int)time(NULL));
	ctx->cseq = rand();
//	ctx->parser = rtsp_parser_create(RTSP_PARSER_CLIENT);

	ctx->status = RTSP_CREATE;
	return ctx;
}

void rtsp_client_destroy(void* rtsp)
{
	int i, r;
	struct rtsp_media_t* media;
	struct rtsp_client_context_t *ctx;

	ctx = (struct rtsp_client_context_t*)rtsp;
	ctx->status = RTSP_DESTROY;
	r = rtsp_client_media_teardown(ctx);

	if(ctx->media_ptr)
		free(ctx->media_ptr);
	return 0;
}

int rtsp_client_open(void* rtsp, const char* uri, const rtsp_client_transport_t *transport, void* ptr)
{
	int r;
	struct rtsp_client_context_t *ctx;
	ctx = (struct rtsp_client_context_t*)rtsp;

	strncpy(ctx->uri, uri, sizeof(ctx->uri)-1);
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
		strncpy(ctx->uri, uri, sizeof(ctx->uri)-1);

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
		if(r = ctx->client.rtpport(ctx->param, &media->transport.client_port1))
			return r;

		media->transport.transport = RTSP_TRANSPORT_RTP;
		if(0 == media->transport.client_port1)
		{
			media->transport.lower_transport = RTSP_TRANSPORT_TCP;
			media->transport.client_port1 = 2*i;
			media->transport.client_port2 = 2*i + 1;
		}
		else
		{
			media->transport.lower_transport = RTSP_TRANSPORT_UDP;
			media->transport.client_port2 = media->transport.client_port1 + 1;
		}
	}

	return rtsp_client_media_setup(ctx);
}

int rtsp_close(void* rtsp)
{
		
}

int rtsp_media_count(void* rtsp)
{
	struct rtsp_client_context_t *ctx;
	ctx = (struct rtsp_client_context_t*)rtsp;
	return ctx->media_count;
}
