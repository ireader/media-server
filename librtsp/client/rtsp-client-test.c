#if defined(_DEBUG) || defined(DEBUG)

#include "rtsp-client.h"
#include "rtsp-client-transport-tcp.h"
#include <assert.h>
#include <stdlib.h>
#include "cstringext.h"
#include "udpsocket.h"
#include "sys/system.h"

#define RTP_PORT_BASE 30000

struct rtsp_client_test_t
{
	void* rtsp;
	void* transport;
	socket_t rtp;
	socket_t rtcp;
	unsigned short port;
};

static int rtsp_create_rtp_socket(socket_t *rtp, socket_t *rtcp, unsigned short *port)
{
	unsigned short i;
	socket_t sock[2];
	assert(0 == RTP_PORT_BASE % 2);
	srand((unsigned int)time(NULL));

	do
	{
		i = rand() % 30000;
		i = i/2*2 + RTP_PORT_BASE;

		sock[0] = udpsocket_create(NULL, i);
		if(socket_invalid == sock[0])
			continue;

		sock[1] = udpsocket_create(NULL, i + 1);
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

static int rtpport(void* transport, unsigned short *rtp)
{
	socket_t sock[2];
	return rtsp_create_rtp_socket(&sock[0], &sock[1], rtp);
}

int onopen(void* ptr, int code, const struct rtsp_transport_t* transport, int count)
{
	int64_t npt = 0;
	struct rtsp_client_test_t *ctx = (struct rtsp_client_test_t *)ptr;
	assert(0 == rtsp_client_play(ctx->rtsp, &npt, NULL));
	return 0;
}

int onclose(void* ptr, int code)
{
	return 0;
}

int onplay(void* ptr, int code, const int64_t *nptbegin, const int64_t *nptend, const double *scale, const struct rtsp_rtp_info_t* rtpinfo, int count)
{
	return 0;
}

int onpause(void* ptr, int code)
{
	return 0;
}

void rtsp_client_test()
{
	int i;
	void* rtsp;
	void* transport;
	rtsp_client_t client;
	struct rtsp_client_test_t *ctx;
	ctx = (struct rtsp_client_test_t*)malloc(sizeof(*ctx));
	memset(ctx, 0, sizeof(*ctx));

	memset(&client, 0, sizeof(client));
	client.request = rtsp_client_tcp_transport_request;
	client.rtpport = rtpport;
	client.onopen = onopen;
	client.onplay = onplay;
	client.onpause = onpause;
	client.onclose = onclose;

	transport = rtsp_client_tcp_transport_create(NULL, NULL);
	assert(transport);

	ctx->rtsp = rtsp_client_create(&client, transport, ctx);
	assert(ctx->rtsp);

	assert(0 == rtsp_client_open(ctx->rtsp, "rtsp://127.0.0.1/sjz.264"));

	for(i = 0; i < 100; i++)
	{
		if(10 == i)
			rtsp_client_pause(ctx->rtsp);
		if(20 == i)
			rtsp_client_play(ctx->rtsp, NULL, NULL);

		system_sleep(1000);
	}

	assert(0 == rtsp_client_close(ctx->rtsp));
	rtsp_client_tcp_transport_destroy(transport);
	rtsp_client_destroy(ctx->rtsp);
	free(ctx);
}

#endif
