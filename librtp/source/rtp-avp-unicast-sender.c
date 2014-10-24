#include "cstringext.h"
#include "rtp-avp-unicast-sender.h"
#include "rtp-transport.h"
#include "sys/locker.h"

struct rtp_packet_header_t
{
	int32_t ref;
};

struct rtp_unicast_sender_t
{
	char ip[32];
	u_short port[2]; // rtp/rtcp port
	aio_socket_t socket[2]; // rtp/rtcp socket
	byte_t data[MAX_UDP_BYTES]; // rtcp receive buffer

	void *members; // rtp source list
	void *senders; // rtp sender list
	struct rtp_source *source; // self info

	locker_t locker;
};

int rtp_avp_udp_sender_init()
{
	return 0;
}

int rtp_avp_udp_sender_cleanup()
{
	return 0;
}

void* rtp_avp_unicast_sender_create(const char* ip, u_short port[2], socket_t socket[2])
{
	struct rtp_unicast_sender_t *ctx;
	ctx = (struct rtp_unicast_sender_t*)malloc(sizeof(*ctx));
	if(!ctx)
		return NULL;

	// set receiver socket buffer
	//socket_setrecvbuf(rtp, 50*1024);

	memset(ctx, 0, sizeof(*ctx));
	locker_create(&ctx->locker);
	ctx->source = rtp_source_create(ssrc);
	ctx->members = rtp_source_list_create();
	ctx->senders = rtp_source_list_create();
	ctx->socket[0] = aio_socket_create(socket[0], 1);
	ctx->socket[1] = aio_socket_create(socket[1], 1);
	if(!ctx->members || !ctx->senders || !ctx->source || !ctx->socket[0] || !ctx->socket[1])
	{
		rtp_avp_unicast_sender_destroy(ctx);
		return NULL;
	}

	strncpy(ctx->ip, ip, sizeof(ctx->ip));
	ctx->port[0] = port[0];
	ctx->port[1] = port[1];

	LIST_INIT_HEAD(&ctx->head);
	return ctx;
}

void rtp_avp_unicast_sender_destroy(void* sender)
{
	struct rtp_unicast_sender_t *ctx;
	ctx = (struct rtp_unicast_sender_t*)sender;
	if(ctx->socket[0])
		aio_socket_destroy(ctx->socket[0]);
	if(ctx->socket[1])
		aio_socket_destroy(ctx->socket[1]);
	if(ctx->members)
		rtp_source_list_destroy(ctx->members);
	if(ctx->senders)
		rtp_source_list_destroy(ctx->senders);
	if(ctx->source)
		rtp_source_release(ctx->source);

	locker_destroy(&ctx->locker);
	free(ctx);
	return 0;
}

static void rtp_avp_udp_sender_onrtp(void* param, int code, size_t bytes)
{
	struct rtp_unicast_sender_t *ctx;
	ctx = (struct rtp_unicast_sender_t*)param;
}

int rtp_avp_unicast_sender_send(void* sender, const void* data, size_t bytes)
{
	struct list_head *pos;
	struct rtp_packet_header_t *packet;
	struct rtp_unicast_sender_t *ctx;

	ctx = (struct rtp_unicast_sender_t*)sender;
	packet = (struct rtp_packet_header_t *)data - 1;

	atomic_increment32(&packet->ref);
	locker_lock(&ctx->locker);
	locker_unlock(&ctx->locker);

	return aio_socket_sendto(ctx->socket[0], ctx->ip, ctx->port[0], data, bytes, rtp_avp_udp_sender_onrtp, ctx);
}
