/// RFC2250 2. Encapsulation of MPEG System and Transport Streams (p3)

#include "rtp-packet.h"
#include "rtp-payload-helper.h"
#include "rtp-payload-internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

static int rtp_decode_ts(void* p, const void* packet, int bytes)
{
	struct rtp_packet_t pkt;
	struct rtp_payload_helper_t *helper;

	helper = (struct rtp_payload_helper_t *)p;
	if (!helper || 0 != rtp_packet_deserialize(&pkt, packet, bytes))
		return -EINVAL;

	// 2.1 RTP header usage(p4)
	// M bit: Set to 1 whenever the timestamp is discontinuous. (such as 
	// might happen when a sender switches from one data
	// source to another).This allows the receiver and any
	// intervening RTP mixers or translators that are synchronizing
	// to the flow to ignore the difference between this timestamp
	// and any previous timestamp in their clock phase detectors.
	if (pkt.rtp.m)
	{
		//TODO: test
		// new frame start
		helper->size = 0; // discard previous packets
		helper->lost = 0;
		helper->flags |= RTP_PAYLOAD_FLAG_PACKET_LOST; // notify source changed
		helper->seq = (uint16_t)pkt.rtp.seq;
		helper->timestamp = pkt.rtp.timestamp;
	}
	else
	{
		rtp_payload_check(helper, &pkt);
	}

	if (helper->lost)
	{
		return 0; // packet discard;
	}

	rtp_payload_write(helper, &pkt);
	return 1; // packet handled
}

struct rtp_payload_decode_t *rtp_ts_decode()
{
	static struct rtp_payload_decode_t decode = {
		rtp_payload_helper_create,
		rtp_payload_helper_destroy,
		rtp_decode_ts,
	};

	return &decode;
}
