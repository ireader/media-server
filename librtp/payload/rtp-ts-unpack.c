/// RFC2250 2. Encapsulation of MPEG System and Transport Streams (p3)

#include "rtp-packet.h"
#include "rtp-payload-internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

struct rtp_decode_ts_t
{
	struct rtp_payload_t handler;
	void* cbparam;

	int flags; // lost packet

	uint16_t seq; // rtp seq
	uint32_t timestamp;

	uint8_t* ptr;
	int size, capacity;
};

static void* rtp_ts_unpack_create(struct rtp_payload_t *handler, void* cbparam)
{
	struct rtp_decode_ts_t *unpacker;
	unpacker = (struct rtp_decode_ts_t *)calloc(1, sizeof(*unpacker));
	if (!unpacker)
		return NULL;

	memcpy(&unpacker->handler, handler, sizeof(unpacker->handler));
	unpacker->cbparam = cbparam;
	unpacker->flags = -1;
	return unpacker;
}

static void rtp_ts_unpack_destroy(void* p)
{
	struct rtp_decode_ts_t *unpacker;
	unpacker = (struct rtp_decode_ts_t *)p;

	if (unpacker->ptr)
		free(unpacker->ptr);
#if defined(_DEBUG) || defined(DEBUG)
	memset(unpacker, 0xCC, sizeof(*unpacker));
#endif
	free(unpacker);
}

static int rtp_ts_unpack_input(void* p, const void* packet, int bytes)
{
	struct rtp_packet_t pkt;
	struct rtp_decode_ts_t *unpacker;

	unpacker = (struct rtp_decode_ts_t *)p;
	if (!unpacker || 0 != rtp_packet_deserialize(&pkt, packet, bytes))
		return -EINVAL;

	if (-1 == unpacker->flags)
	{
		unpacker->flags = 0;
		unpacker->seq = (uint16_t)(pkt.rtp.seq - 1); // disable packet lost
		unpacker->timestamp = pkt.rtp.timestamp + 1; // flag for new frame
	}

	if ((uint16_t)pkt.rtp.seq != (uint16_t)(unpacker->seq + 1))
	{
		unpacker->flags |= RTP_PAYLOAD_FLAG_PACKET_LOST | RTP_PAYLOAD_FLAG_PACKET_SYNC;
		unpacker->timestamp = pkt.rtp.timestamp;
	}
	unpacker->seq = (uint16_t)pkt.rtp.seq;

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
		unpacker->size = 0; // discard previous packets
		unpacker->flags |= RTP_PAYLOAD_FLAG_PACKET_LOST; // notify source changed
		unpacker->flags &= ~RTP_PAYLOAD_FLAG_PACKET_SYNC; // new frame start
		pkt.rtp.timestamp = unpacker->timestamp;
	}

	if (pkt.rtp.timestamp != unpacker->timestamp)
	{
		if (unpacker->size > 0)
		{
			// previous packet done
			assert(0 == (unpacker->flags & RTP_PAYLOAD_FLAG_PACKET_SYNC));
			unpacker->handler.packet(unpacker->cbparam, unpacker->ptr, unpacker->size, unpacker->timestamp, unpacker->flags);
			unpacker->flags &= ~RTP_PAYLOAD_FLAG_PACKET_LOST; // clear packet lost flag
		}

		// new frame start
		unpacker->flags &= ~RTP_PAYLOAD_FLAG_PACKET_SYNC;
		unpacker->size = 0;
	}
	unpacker->timestamp = pkt.rtp.timestamp;

	if (0 != (unpacker->flags & RTP_PAYLOAD_FLAG_PACKET_SYNC))
	{
		return 0; // packet discard
	}

	// save payload
	assert(pkt.payloadlen > 0);
	if (pkt.payload && pkt.payloadlen > 0)
	{
		if (unpacker->size + pkt.payloadlen > unpacker->capacity)
		{
			size_t size = unpacker->size + pkt.payloadlen + 160 * 188;
			void *ptr = realloc(unpacker->ptr, size);
			if (!ptr)
			{
				unpacker->flags |= RTP_PAYLOAD_FLAG_PACKET_LOST | RTP_PAYLOAD_FLAG_PACKET_SYNC;
				unpacker->size = 0;
				return -ENOMEM;
			}

			unpacker->ptr = (uint8_t*)ptr;
			unpacker->capacity = size;
		}

		assert(unpacker->capacity >= unpacker->size + pkt.payloadlen);
		memcpy(unpacker->ptr + unpacker->size, pkt.payload, pkt.payloadlen);
		unpacker->size += pkt.payloadlen;
	}

	return 1; // packet handled
}

struct rtp_payload_decode_t *rtp_ts_decode()
{
	static struct rtp_payload_decode_t decode = {
		rtp_ts_unpack_create,
		rtp_ts_unpack_destroy,
		rtp_ts_unpack_input,
	};

	return &decode;
}
