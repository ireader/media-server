#include "ctypedef.h"
#include "rtp-unpack.h"
#include "rtp-packet.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct rtp_unpacker_t
{
	struct rtp_unpack_func_t func;
	void* cbparam;

	uint16_t seq; // RTP seq
};

static void* rtp_unpack_create(struct rtp_unpack_func_t *func, void* param)
{
	struct rtp_unpacker_t *unpacker;
	unpacker = (struct rtp_unpacker_t *)malloc(sizeof(*unpacker));
	if (!unpacker)
		return NULL;

	memset(unpacker, 0, sizeof(*unpacker));
	memcpy(&unpacker->func, func, sizeof(unpacker->func));
	unpacker->cbparam = param;
	return unpacker;
}

static void rtp_unpack_destroy(void* p)
{
	struct rtp_unpacker_t *unpacker;
	unpacker = (struct rtp_unpacker_t *)p;
#if defined(_DEBUG) || defined(DEBUG)
	memset(unpacker, 0xCC, sizeof(*unpacker));
#endif
	free(unpacker);
}

static int rtp_unpack_input(void* p, const void* packet, size_t bytes, uint64_t time)
{
	int discontinue;
	struct rtp_packet_t pkt;
	struct rtp_unpacker_t *unpacker;

	unpacker = (struct rtp_unpacker_t *)p;
	if (!unpacker || 0 != rtp_packet_deserialize(&pkt, packet, bytes))
		return -1;

	discontinue = (uint16_t)pkt.rtp.seq != (unpacker->seq + 1) ? 1 : 0;
	unpacker->func.packet(unpacker->cbparam, pkt.payload, pkt.payloadlen, time, discontinue);
	unpacker->seq = (uint16_t)pkt.rtp.seq;
	return 0;
}

struct rtp_unpack_t *rtp_unpacker()
{
	static struct rtp_unpack_t unpacker = {
		rtp_unpack_create,
		rtp_unpack_destroy,
		rtp_unpack_input,
	};

	return &unpacker;
}
