/// RFC2250 3. Encapsulation of MPEG Elementary Streams (p4)

#include "rtp-packet.h"
#include "rtp-profile.h"
#include "rtp-payload-internal.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#define MPEG2VIDEO_EXTENSION_HEADER 0x04

struct rtp_decode_mpeg2es_t
{
	struct rtp_payload_t handler;
	void* cbparam;

	uint16_t seq; // rtp seq
	uint32_t timestamp;

	uint8_t* ptr;
	int size, capacity;

	int flags;
};

static void* rtp_mpeg2es_unpack_create(struct rtp_payload_t *handler, void* param)
{
	struct rtp_decode_mpeg2es_t *unpacker;
	unpacker = (struct rtp_decode_mpeg2es_t *)calloc(1, sizeof(*unpacker));
	if (!unpacker)
		return NULL;

	memcpy(&unpacker->handler, handler, sizeof(unpacker->handler));
	unpacker->cbparam = param;
	unpacker->flags = -1;
	return unpacker;
}

static void rtp_mpeg2es_unpack_destroy(void* p)
{
	struct rtp_decode_mpeg2es_t *unpacker;
	unpacker = (struct rtp_decode_mpeg2es_t *)p;

	if (unpacker->ptr)
		free(unpacker->ptr);
#if defined(_DEBUG) || defined(DEBUG)
	memset(unpacker, 0xCC, sizeof(*unpacker));
#endif
	free(unpacker);
}
static int rtp_mpeg2es_unpack_input(void* p, const void* packet, int bytes)
{
	int n;
	struct rtp_packet_t pkt;
	struct rtp_decode_mpeg2es_t *unpacker;

	unpacker = (struct rtp_decode_mpeg2es_t *)p;
	if (!unpacker || 0 != rtp_packet_deserialize(&pkt, packet, bytes) || pkt.payloadlen < 4)
		return -EINVAL;

	if (RTP_PAYLOAD_MPA != pkt.rtp.pt && RTP_PAYLOAD_MPV != pkt.rtp.pt)
	{
		assert(0);
		return -EINVAL;
	}

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

	if (pkt.rtp.timestamp != unpacker->timestamp)
	{
		if (unpacker->size > 0)
		{
			// previous packet done
			assert(RTP_PAYLOAD_MPA == pkt.rtp.pt);
			unpacker->handler.packet(unpacker->cbparam, unpacker->ptr, unpacker->size, unpacker->timestamp, unpacker->flags);
			unpacker->flags &= ~RTP_PAYLOAD_FLAG_PACKET_LOST; // clear packet lost flag
		}

		// new frame start
		unpacker->flags &= ~RTP_PAYLOAD_FLAG_PACKET_SYNC;
		unpacker->size = 0;
	}
	unpacker->timestamp = pkt.rtp.timestamp;

	// save payload
	assert(pkt.payloadlen > 0);
	if (0 == (unpacker->flags & RTP_PAYLOAD_FLAG_PACKET_SYNC) && pkt.payload && pkt.payloadlen > 0)
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

		n = 4; // skip 3.4 MPEG Video-specific header
		if (RTP_PAYLOAD_MPV == pkt.rtp.pt && (((uint8_t*)pkt.payload)[4] & MPEG2VIDEO_EXTENSION_HEADER))
			n += 4; // 3.4.1 MPEG-2 Video-specific header extension
		assert(unpacker->capacity >= unpacker->size + pkt.payloadlen);
		memcpy(unpacker->ptr + unpacker->size, (uint8_t*)pkt.payload + n, pkt.payloadlen - n);
		unpacker->size += pkt.payloadlen - n;
	}

	// M bit: For video, set to 1 on packet containing MPEG frame end code, 0 otherwise.
	//        For audio, set to 1 on first packet of a "talk-spurt," 0 otherwise.
	if (pkt.rtp.m && RTP_PAYLOAD_MPV == pkt.rtp.pt)
	{
		if (unpacker->size > 0)
		{
			// previous packet done
			assert(0 == (unpacker->flags & RTP_PAYLOAD_FLAG_PACKET_SYNC));
			unpacker->handler.packet(unpacker->cbparam, unpacker->ptr, unpacker->size, unpacker->timestamp, unpacker->flags);
			unpacker->flags &= ~RTP_PAYLOAD_FLAG_PACKET_LOST; // clear packet lost flag
		}

		unpacker->size = 0; // discard previous packets
		unpacker->flags &= ~RTP_PAYLOAD_FLAG_PACKET_SYNC; // new frame start
	}

	return (unpacker->flags & RTP_PAYLOAD_FLAG_PACKET_SYNC) ? 0 : 1; // packet handled
}

struct rtp_payload_decode_t *rtp_mpeg1or2es_decode()
{
	static struct rtp_payload_decode_t unpacker = {
		rtp_mpeg2es_unpack_create,
		rtp_mpeg2es_unpack_destroy,
		rtp_mpeg2es_unpack_input,
	};

	return &unpacker;
}
