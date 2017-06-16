#if defined(_DEBUG) || defined(DEBUG)

#include "sockutil.h"
#include "rtsp-client.h"
#include "rtsp-parser.h"
#include <assert.h>
#include <stdlib.h>
#include "rtp-socket.h"
#include "cstringext.h"
#include "sys/system.h"
#include "cpm/unuse.h"

struct rtsp_client_test_t
{
	void* rtsp;
	socket_t socket;

	int media;
	socket_t rtp[5][2];
	unsigned short port[5][2];
};

static int rtsp_client_send(void* param, const char* uri, const void* req, size_t bytes)
{
	//TODO: check uri and make socket
	//1. uri != rtsp describe uri(user input)
	//2. multi-uri if media_count > 1
	struct rtsp_client_test_t *ctx = (struct rtsp_client_test_t *)param;
	return socket_send_all_by_time(ctx->socket, req, bytes, 0, 2000);
}

static int rtpport(void* param, unsigned short *rtp)
{
	struct rtsp_client_test_t *ctx = (struct rtsp_client_test_t *)param;
	assert(0 == rtp_socket_create(NULL, ctx->rtp[ctx->media], ctx->port[ctx->media]));
	*rtp = ctx->port[ctx->media][0];
	++ctx->media;
	return 0;
}

int onopen(void* param)
{
	int i, count;
	uint64_t npt = 0;
	struct rtsp_client_test_t *ctx = (struct rtsp_client_test_t *)param;
	assert(0 == rtsp_client_play(ctx->rtsp, &npt, NULL));
	count = rtsp_client_media_count(ctx->rtsp);
	assert(count == ctx->media);
	for (i = 0; i < count; i++)
	{
		int payload, port[2];
		const char* encoding;
		const struct rtsp_header_transport_t* transport;
		transport = rtsp_client_get_media_transport(ctx->rtsp, i);
		encoding = rtsp_client_get_media_encoding(ctx->rtsp, i);
		payload = rtsp_client_get_media_payload(ctx->rtsp, i);
		assert(RTSP_TRANSPORT_RTP_UDP == transport->transport); // udp only
		assert(0 == transport->multicast); // unicast only
		assert(transport->rtp.u.client_port1 == ctx->port[i][0]);
		assert(transport->rtp.u.client_port2 == ctx->port[i][1]);

		port[0] = transport->rtp.u.server_port1;
		port[1] = transport->rtp.u.server_port2;
		rtp_receiver_test(ctx->rtp[i], transport->source, port, payload, encoding);
	}
	return 0;
}

int onclose(void* param)
{
	return 0;
}

int onplay(void* param, int media, const uint64_t *nptbegin, const uint64_t *nptend, const double *scale, const struct rtsp_rtp_info_t* rtpinfo, int count)
{
	return 0;
}

int onpause(void* param)
{
	return 0;
}

void rtsp_client_test(const char* host, const char* file)
{
	int r, n, ret;
	void* parser;
	struct rtsp_client_test_t ctx;
	struct rtsp_client_handler_t handler;
	static char packet[2 * 1024 * 1024];

	memset(&ctx, 0, sizeof(ctx));
	handler.send = rtsp_client_send;
	handler.rtpport = rtpport;
	handler.onopen = onopen;
	handler.onplay = onplay;
	handler.onpause = onpause;
	handler.onclose = onclose;

	parser = rtsp_parser_create(RTSP_PARSER_CLIENT);
	snprintf(packet, sizeof(packet), "rtsp://%s/%s", host, file); // url

	socket_init();
	ctx.socket = socket_connect_host(host, 554, 2000);
	assert(socket_invalid != ctx.socket);
	ctx.rtsp = rtsp_client_create(&handler, &ctx);
	assert(ctx.rtsp);
	assert(0 == rtsp_client_open(ctx.rtsp, packet, NULL));

	socket_setnonblock(ctx.socket, 0);
	r = socket_recv(ctx.socket, packet, sizeof(packet), 0);
	while(r > 0)
	{
		n = r;
		ret = rtsp_parser_input(parser, packet, &r);
		assert(0 == ret || 1 == ret);
		while (0 == ret)
		{
			assert(0 == rtsp_client_input(ctx.rtsp, parser));

			// next round
			rtsp_parser_clear(parser);
			ret = rtsp_parser_input(parser, packet + (n - r), &r);
			assert(0 == ret || 1 == ret);
		}

		r = socket_recv(ctx.socket, packet, sizeof(packet), 0);
	}

	assert(0 == rtsp_client_close(ctx.rtsp));
	rtsp_client_destroy(ctx.rtsp);
	socket_close(ctx.socket);
	socket_cleanup();
}

#endif
