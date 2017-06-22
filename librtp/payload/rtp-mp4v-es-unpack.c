/// RFC6416 RTP Payload Format for MPEG-4 Audio/Visual Streams

#include "rtp-packet.h"
#include "rtp-payload-helper.h"
#include "rtp-payload-internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

static int rtp_decode_mp4v_es(void* p, const void* packet, int bytes)
{
	struct rtp_packet_t pkt;
	struct rtp_payload_helper_t *helper;

	helper = (struct rtp_payload_helper_t *)p;
	if (!helper || 0 != rtp_packet_deserialize(&pkt, packet, bytes))
		return -EINVAL;

	rtp_payload_check(helper, &pkt);

	// save payload
	assert(pkt.payloadlen > 0);
	if (!helper->lost && pkt.payload && pkt.payloadlen > 0)
	{
		if (0 != rtp_payload_write(helper, &pkt))
			return -ENOMEM;
	}

	// 5.1. Use of RTP Header Fields for MPEG-4 Visual (p9)
	// Marker (M) bit: The marker bit is set to 1 to indicate the last RTP
	// packet(or only RTP packet) of a VOP.When multiple VOPs are carried
	// in the same RTP packet, the marker bit is set to 1.
	if (pkt.rtp.m)
	{
		rtp_payload_onframe(helper);
	}

	return helper->lost ? 0 : 1; // packet handled
}

struct rtp_payload_decode_t *rtp_mp4v_es_decode()
{
	static struct rtp_payload_decode_t decode = {
		rtp_payload_helper_create,
		rtp_payload_helper_destroy,
		rtp_decode_mp4v_es,
	};

	return &decode;
}
