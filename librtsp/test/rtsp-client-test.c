#if defined(_DEBUG) || defined(DEBUG)

#include "sockutil.h"
#include "rtsp-client.h"
#include <assert.h>
#include <stdlib.h>
#include "sockpair.h"
#include "cstringext.h"
#include "sys/system.h"
#include "cpm/unuse.h"

void rtp_receiver_tcp_input(uint8_t channel, const void* data, uint16_t bytes);
void rtp_receiver_test(socket_t rtp[2], const char* peer, int peerport[2], int payload, const char* encoding);
void* rtp_receiver_tcp_test(uint8_t interleave1, uint8_t interleave2, int payload, const char* encoding);

struct rtsp_client_test_t
{
	void* rtsp;
	socket_t socket;

	int transport;
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

static int rtpport(void* param, int media, unsigned short *rtp)
{
	struct rtsp_client_test_t *ctx = (struct rtsp_client_test_t *)param;
	switch (ctx->transport)
	{
	case RTSP_TRANSPORT_RTP_UDP:
		assert(0 == sockpair_create(NULL, ctx->rtp[media], ctx->port[media]));
		*rtp = ctx->port[media][0];
		break;

	case RTSP_TRANSPORT_RTP_TCP:
		*rtp = 0;
		break;

	default:
		assert(0);
		return -1;
	}

	return 0;
}

int rtsp_client_options(rtsp_client_t *rtsp, const char* commands);
static void onrtp(void* param, uint8_t channel, const void* data, uint16_t bytes)
{
	static int keepalive = 0;
	struct rtsp_client_test_t *ctx = (struct rtsp_client_test_t *)param;
	rtp_receiver_tcp_input(channel, data, bytes);
	if (++keepalive % 1000 == 0)
	{
		rtsp_client_play(ctx->rtsp, NULL, NULL);
	}
}

static int ondescribe(void* param, const char* sdp)
{
	struct rtsp_client_test_t *ctx = (struct rtsp_client_test_t *)param;
	return rtsp_client_setup(ctx->rtsp, sdp);
}

static int onsetup(void* param)
{
	int i;
	uint64_t npt = 0;
	char ip[65];
	u_short rtspport;
	struct rtsp_client_test_t *ctx = (struct rtsp_client_test_t *)param;
	assert(0 == rtsp_client_play(ctx->rtsp, &npt, NULL));
	for (i = 0; i < rtsp_client_media_count(ctx->rtsp); i++)
	{
		int payload, port[2];
		const char* encoding;
		const struct rtsp_header_transport_t* transport;
		transport = rtsp_client_get_media_transport(ctx->rtsp, i);
		encoding = rtsp_client_get_media_encoding(ctx->rtsp, i);
		payload = rtsp_client_get_media_payload(ctx->rtsp, i);
		if (RTSP_TRANSPORT_RTP_UDP == transport->transport)
		{
			//assert(RTSP_TRANSPORT_RTP_UDP == transport->transport); // udp only
			assert(0 == transport->multicast); // unicast only
			assert(transport->rtp.u.client_port1 == ctx->port[i][0]);
			assert(transport->rtp.u.client_port2 == ctx->port[i][1]);

			port[0] = transport->rtp.u.server_port1;
			port[1] = transport->rtp.u.server_port2;
			if (*transport->source)
			{
				rtp_receiver_test(ctx->rtp[i], transport->source, port, payload, encoding);
			}
			else
			{
				socket_getpeername(ctx->socket, ip, &rtspport);
				rtp_receiver_test(ctx->rtp[i], ip, port, payload, encoding);
			}
		}
		else if (RTSP_TRANSPORT_RTP_TCP == transport->transport)
		{
			//assert(transport->rtp.u.client_port1 == transport->interleaved1);
			//assert(transport->rtp.u.client_port2 == transport->interleaved2);
			rtp_receiver_tcp_test(transport->interleaved1, transport->interleaved2, payload, encoding);
		}
		else
		{
			assert(0); // TODO
		}
	}

	return 0;
}

static int onteardown(void* param)
{
	return 0;
}

static int onplay(void* param, int media, const uint64_t *nptbegin, const uint64_t *nptend, const double *scale, const struct rtsp_rtp_info_t* rtpinfo, int count)
{
	return 0;
}

static int onpause(void* param)
{
	return 0;
}

void rtsp_client_test(const char* host, const char* file)
{
	int r;
	struct rtsp_client_test_t ctx;
	struct rtsp_client_handler_t handler;
	static char packet[2 * 1024 * 1024];

	memset(&ctx, 0, sizeof(ctx));
	handler.send = rtsp_client_send;
	handler.rtpport = rtpport;
	handler.ondescribe = ondescribe;
	handler.onsetup = onsetup;
	handler.onplay = onplay;
	handler.onpause = onpause;
	handler.onteardown = onteardown;
	handler.onrtp = onrtp;

	ctx.transport = RTSP_TRANSPORT_RTP_UDP; // RTSP_TRANSPORT_RTP_TCP
	snprintf(packet, sizeof(packet), "rtsp://%s/%s", host, file); // url

	socket_init();
	ctx.socket = socket_connect_host(host, 554, 2000);
	assert(socket_invalid != ctx.socket);
	//ctx.rtsp = rtsp_client_create(NULL, NULL, &handler, &ctx);
	ctx.rtsp = rtsp_client_create(packet, "username1", "password1", &handler, &ctx);
	assert(ctx.rtsp);
	assert(0 == rtsp_client_describe(ctx.rtsp));

	socket_setnonblock(ctx.socket, 0);
	r = socket_recv(ctx.socket, packet, sizeof(packet), 0);
	while(r > 0)
	{
		assert(0 == rtsp_client_input(ctx.rtsp, packet, r));
		r = socket_recv(ctx.socket, packet, sizeof(packet), 0);
	}

	assert(0 == rtsp_client_teardown(ctx.rtsp));
	rtsp_client_destroy(ctx.rtsp);
	socket_close(ctx.socket);
	socket_cleanup();
}

#endif
