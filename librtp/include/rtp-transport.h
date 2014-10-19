#ifndef _rtp_transport_h_
#define _rtp_transport_h_

#include "rtp.h"
#include "rtcp.h"
#include "rtp-source-list.h"
#include "sys/sock.h"
#include "aio-socket.h"

#define MAX_UDP_BYTES		8192
#define PERFECT_UDP_BYTES	512

typedef unsigned char byte_t;

struct rtp_transport
{
	aio_socket_t socket;
	void* data;
};

struct rtcp_transport
{
	aio_socket_t socket;
	byte_t data[MAX_UDP_BYTES];
};

struct rtp_context
{
	void* queue;
	struct rtp_transport rtp;
	struct rtcp_transport rtcp;

	void *members; // rtp source list
	void *senders; // rtp sender list
	struct rtp_source info; // self info
};

void rtcp_input_rtp(struct rtp_context *ctx, const void* data, size_t bytes);
void rtcp_input_rtcp(struct rtp_context *ctx, const void* data, size_t bytes);

int rtcp_sender_report(struct rtp_context *ctx, void* data, size_t bytes);

int rtcp_receiver_report(struct rtp_context *ctx, void* data, size_t bytes);

int rtcp_sdes(struct rtp_context *ctx, void* data, size_t bytes);

#endif /* !_rtp_transport_h_ */
