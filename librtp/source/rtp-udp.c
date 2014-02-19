#include "cstringext.h"
#include "sys/sock.h"
#include "sys/system.h"
#include "sys/process.h"
#include "aio-socket.h"
#include "thread-pool.h"
#include "rtp-avp-udp.h"
#include "rtp-transport.h"
#include <stdio.h>

void* rtp_avp_udp_create(int rtp, int rtcp, void* queue)
{
	struct rtp_context *ctx;
	ctx = (struct rtp_context*)malloc(sizeof(struct rtp_context));
	if(!ctx)
		return NULL;

	memset(ctx, 0, sizeof(struct rtp_context));
	ctx->members = rtp_source_list_create();
	ctx->senders = rtp_source_list_create();
	if(!ctx->members || !ctx->senders)
	{
		free(ctx);
		return NULL;
	}

	ctx->queue = queue;
	ctx->rtp.socket = aio_socket_create((socket_t)rtp, 1);
	ctx->rtcp.socket = aio_socket_create((socket_t)rtcp, 1);
	return ctx;
}

int rtp_avp_udp_destroy(void* udp)
{
	struct rtp_context *ctx;
	ctx = (struct rtp_context*)udp;
	if(ctx->rtp.socket)
		aio_socket_destroy(ctx->rtp.socket);
	if(ctx->rtcp.socket)
		aio_socket_destroy(ctx->rtcp.socket);
	if(ctx->members)
		rtp_source_list_destroy(ctx->members);
	if(ctx->senders)
		rtp_source_list_destroy(ctx->senders);
	return 0;
}

static void rtp_avp_udp_onrtp(void* param, int code, int bytes, const char* ip, int port)
{
	int r;
	rtp_header_t *rtphd;
	struct rtp_context *ctx;
	ctx = (struct rtp_context*)param;
	rtphd = (rtp_header_t*)ctx->rtp.data;
	if(0 == code)
	{
		printf("rtp data: %d[%s.%d]\n", bytes, ip, port);

		// RTCP statistics
		rtcp_input_rtp(ctx, ctx->rtp.data, bytes);

		// input queue
		r = rtp_queue_unlock(ctx->queue, ctx->rtp.data, bytes);
		if(0 == r)
		{
			r = rtp_queue_lock(ctx->queue, &ctx->rtp.data, MAX_UDP_BYTES);
			if(0 == r)
			{
				r = aio_socket_recvfrom(ctx->rtp.socket, ctx->rtp.data, MAX_UDP_BYTES, rtp_avp_udp_onrtp, ctx);
				if(0 != r)
					rtp_queue_unlock(ctx->queue, ctx->rtp.data, -1);
			}
		}

		if(0 != r)
		{
			printf("rtp_avp_udp_onrtp ---: %d\n", r);
		}
	}
	else
	{
		r = rtp_queue_unlock(ctx->queue, ctx->rtp.data, -1);
		printf("rtp_avp_udp_onrtp error: %d\n", code);
	}
}

static void rtp_avp_udp_onrtcp(void* param, int code, int bytes, const char* ip, int port)
{
	struct rtp_context *ctx;
	printf("rtcp data: %d[%s.%d]\n", bytes, ip, port);

	if(0 == code)
	{
		ctx = (struct rtp_context*)param;
		rtcp_input_rtcp(ctx, ctx->rtcp.data, bytes);

		code = aio_socket_recvfrom(ctx->rtcp.socket, ctx->rtcp.data, sizeof(ctx->rtcp.data), rtp_avp_udp_onrtcp, ctx);
	}

	if(0 != code)
		printf("rtp_avp_udp_onrtcp error: %d\n", code);
}

int rtp_avp_udp_start(void* udp)
{
	int r;
	struct rtp_context *ctx;

	ctx = (struct rtp_context*)udp;
	r = rtp_queue_lock(ctx->queue, &ctx->rtp.data, MAX_UDP_BYTES);
	if(0 == r)
	{
		r = aio_socket_recvfrom(ctx->rtp.socket, ctx->rtp.data, MAX_UDP_BYTES, rtp_avp_udp_onrtp, ctx);
		if(0 != r)
			rtp_queue_unlock(ctx->queue, ctx->rtp.data, -1);
	}

	r = aio_socket_recvfrom(ctx->rtcp.socket, ctx->rtcp.data, sizeof(ctx->rtcp.data), rtp_avp_udp_onrtcp, ctx);
	return r;
}

int rtp_avp_udp_pause(void* udp)
{
	struct rtp_context *ctx;
	ctx = (struct rtp_context*)udp;
	return 0;
}

int rtp_avp_udp_stop(void* udp)
{
	struct rtp_context *ctx;
	ctx = (struct rtp_context*)udp;
	return 0;
}

static thread_t s_threads[64];
static thread_pool_t g_thpool;
static int STDCALL rtp_avp_thread(void *param)
{
	while(1)
	{
		aio_socket_process(2*60*1000);
	}
	return 0;
}

int rtp_avp_init(void)
{
	size_t i;
	socket_init();
	//r = system_getcpucount();
	aio_socket_init(2);
	for(i=0; i<2 && i<sizeof(s_threads)/sizeof(s_threads[0]); i++)
		thread_create(&s_threads[i], rtp_avp_thread, NULL);
	return 0;
}

int rtp_avp_cleanup(void)
{
	return 0;
}
