// RFC3551 RTP Profile for Audio and Video Conferences with Minimal Control

#include "rtp-packet.h"
#include "rtp-profile.h"
#include "rtp-payload-helper.h"
#include "rtp-payload-internal.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

static int rtp_decode_rfc2250(void* p, const void* packet, int bytes)
{
	struct rtp_packet_t pkt;
	struct rtp_payload_helper_t *helper;

	helper = (struct rtp_payload_helper_t *)p;
	if (!helper || 0 != rtp_packet_deserialize(&pkt, packet, bytes) || pkt.payloadlen < 1)
		return -EINVAL;

	assert(pkt.payloadlen > 0);
	helper->handler.packet(helper->cbparam, pkt.payload, pkt.payloadlen, pkt.rtp.timestamp, 0);
	return 1; // packet handled
}

struct rtp_payload_decode_t *rtp_common_decode()
{
	static struct rtp_payload_decode_t unpacker = {
		rtp_payload_helper_create,
		rtp_payload_helper_destroy,
		rtp_decode_rfc2250,
	};

	return &unpacker;
}
