#include "cstringext.h"
#include "rtp-avp-udp-receiver.h"
#include "rtp-internal.h"
#include <assert.h>

static void rtp_avp_udp_receiver_onrtcp(void* param, int code, size_t bytes, const char* ip, int port)
{
	struct rtp_context *ctx;

	if(0 == code)
	{
		ctx = (struct rtp_context*)param;
		rtcp_input_rtcp(ctx, ctx->rtcp.data, bytes);

		code = aio_socket_recvfrom(ctx->rtcp.socket, ctx->rtcp.data, sizeof(ctx->rtcp.data), rtp_avp_udp_receiver_onrtcp, ctx);
	}

	if(0 != code)
		printf("rtp_avp_udp_onrtcp error: %d\n", code);
}

void* rtp_avp_udp_receiver_create(socket_t rtp, socket_t rtcp, rtp_avp_udp_onrecv callback, void* param)
{
	struct rtp_context *ctx;
	ctx = (struct rtp_context*)malloc(sizeof(struct rtp_context));
	if(!ctx)
		return NULL;

	memset(ctx, 0, sizeof(struct rtp_context));
	ctx->members = rtp_member_list_create();
	ctx->senders = rtp_member_list_create();
	if(!ctx->members || !ctx->senders)
	{
		free(ctx);
		return NULL;
	}

	// set receiver socket buffer
	//socket_setrecvbuf(rtp, 50*1024);

	ctx->queue = queue;
	ctx->rtp.socket = aio_socket_create((socket_t)rtp, 1);
	ctx->rtcp.socket = aio_socket_create((socket_t)rtcp, 1);
	return ctx;
}

void rtp_avp_udp_receiver_destroy(void* receiver)
{
	struct rtp_context *ctx;
	ctx = (struct rtp_context*)receiver;
	if(ctx->rtp.socket)
		aio_socket_destroy(ctx->rtp.socket);
	if(ctx->rtcp.socket)
		aio_socket_destroy(ctx->rtcp.socket);
	if(ctx->members)
		rtp_member_list_destroy(ctx->members);
	if(ctx->senders)
		rtp_member_list_destroy(ctx->senders);
	return 0;
}
