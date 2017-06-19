/// RFC6416 RTP Payload Format for MPEG-4 Audio/Visual Streams

#include "rtp-packet.h"
#include "rtp-payload-helper.h"
#include "rtp-payload-internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

static int rtp_decode_mp4ves(void* p, const void* packet, int bytes)
{
	struct rtp_packet_t pkt;
	struct rtp_payload_helper_t *helper;

	int n;
	static int s_i;
	const uint8_t pp[] = { 0xb6, 0x15, 0x60 };
	const uint8_t*pm;

	helper = (struct rtp_payload_helper_t *)p;
	if (!helper || 0 != rtp_packet_deserialize(&pkt, packet, bytes))
		return -EINVAL;

	//++s_i;
	//for (pm = pkt.payload; pm < (uint8_t*)pkt.payload + pkt.payloadlen;)
	//{
	//	uint8_t* p1 = (uint8_t*)memchr(pm, 0xb6, (uint8_t*)pkt.payload + pkt.payloadlen - pm);
	//	if (p1)
	//	{
	//		for (n = 1; n < sizeof(pp); n++)
	//		{
	//			if (p1[n] != pp[n])
	//				break;
	//		}
	//		if (n == sizeof(pp))
	//			break;
	//		pm = p1 + 1;
	//	}
	//	else
	//	{
	//		break;
	//	}
	//}

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

struct rtp_payload_decode_t *rtp_mp4ves_decode()
{
	static struct rtp_payload_decode_t decode = {
		rtp_payload_helper_create,
		rtp_payload_helper_destroy,
		rtp_decode_mp4ves,
	};

	return &decode;
}
