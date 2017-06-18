// RFC3551 RTP Prole for Audio and Video Conferences with Minimal Control

#include "ctypedef.h"
#include "rtp-packet.h"
#include "rtp-profile.h"
#include "rtp-payload-internal.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct rtp_unpacker_t
{
	struct rtp_payload_t handler;
	void* cbparam;

	int flags;
	uint16_t seq; // RTP seq
};

static void* rtp_unpack_create(struct rtp_payload_t *handler, void* param)
{
	struct rtp_unpacker_t *unpacker;
	unpacker = (struct rtp_unpacker_t *)calloc(1, sizeof(*unpacker));
	if (!unpacker)
		return NULL;

	memcpy(&unpacker->handler, handler, sizeof(unpacker->handler));
	unpacker->cbparam = param;
	unpacker->flags = -1;
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

static int rtp_unpack_input(void* p, const void* packet, int bytes)
{
	struct rtp_packet_t pkt;
	struct rtp_unpacker_t *unpacker;

	unpacker = (struct rtp_unpacker_t *)p;
	if (!unpacker || 0 != rtp_packet_deserialize(&pkt, packet, bytes))
		return -EINVAL;

	if (-1 == unpacker->flags)
	{
		unpacker->flags = 0;
		unpacker->seq = (uint16_t)(pkt.rtp.seq - 1); // disable packet lost
	}

	if ((uint16_t)pkt.rtp.seq != (uint16_t)(unpacker->seq + 1))
	{
		unpacker->flags = RTP_PAYLOAD_FLAG_PACKET_LOST;
	}

	assert(pkt.payloadlen > 0);
	unpacker->handler.packet(unpacker->cbparam, pkt.payload, pkt.payloadlen, pkt.rtp.timestamp, unpacker->flags);
	unpacker->seq = (uint16_t)pkt.rtp.seq;
	unpacker->flags = 0;
	return 1; // packet handled
}

struct rtp_payload_decode_t *rtp_common_decode()
{
	static struct rtp_payload_decode_t unpacker = {
		rtp_unpack_create,
		rtp_unpack_destroy,
		rtp_unpack_input,
	};

	return &unpacker;
}
