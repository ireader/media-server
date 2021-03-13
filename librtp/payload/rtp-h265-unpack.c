// RFC7798 RTP Payload Format for High Efficiency Video Coding (HEVC)

#include "rtp-packet.h"
#include "rtp-payload-internal.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

/*
0               1
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|F|    Type   |  LayerId  | TID |
+-------------+-----------------+

Forbidden zero(F) : 1 bit
NAL unit type(Type) : 6 bits
NUH layer ID(LayerId) : 6 bits
NUH temporal ID plus 1 (TID) : 3 bits
*/

#define H265_TYPE(v) ((v >> 1) & 0x3f)

#define FU_START(v) (v & 0x80)
#define FU_END(v)	(v & 0x40)
#define FU_NAL(v)	(v & 0x3F)

struct rtp_decode_h265_t
{
	struct rtp_payload_t handler;
	void* cbparam;

	uint16_t seq; // rtp seq
	uint32_t timestamp;

	uint8_t* ptr;
	int size, capacity;

	int flags;
	int using_donl_field;
};

static void* rtp_h265_unpack_create(struct rtp_payload_t *handler, void* param)
{
	struct rtp_decode_h265_t *unpacker;
	unpacker = (struct rtp_decode_h265_t *)calloc(1, sizeof(*unpacker));
	if (!unpacker)
		return NULL;

	memcpy(&unpacker->handler, handler, sizeof(unpacker->handler));
	unpacker->cbparam = param;
	unpacker->flags = -1;
	return unpacker;
}

static void rtp_h265_unpack_destroy(void* p)
{
	struct rtp_decode_h265_t *unpacker;
	unpacker = (struct rtp_decode_h265_t *)p;

	if (unpacker->ptr)
		free(unpacker->ptr);
#if defined(_DEBUG) || defined(DEBUG)
	memset(unpacker, 0xCC, sizeof(*unpacker));
#endif
	free(unpacker);
}

// 4.4.2. Aggregation Packets (APs) (p25)
/*
 0               1               2               3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                          RTP Header                           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|      PayloadHdr (Type=48)     |           NALU 1 DONL         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|           NALU 1 Size         |            NALU 1 HDR         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
|                         NALU 1 Data . . .                     |
|                                                               |
+     . . .     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|               |  NALU 2 DOND  |            NALU 2 Size        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|          NALU 2 HDR           |                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+            NALU 2 Data        |
|                                                               |
|         . . .                 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                               :    ...OPTIONAL RTP padding    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
static int rtp_h265_unpack_ap(struct rtp_decode_h265_t *unpacker, const uint8_t* ptr, int bytes, uint32_t timestamp)
{
	int r;
	int n;
	int len;
	//uint16_t donl;
	//uint16_t dond;

	//donl = unpacker->using_donl_field ? nbo_r16(ptr + 2) : 0;
	ptr += 2; // PayloadHdr
	n = 2 /*LEN*/ + (unpacker->using_donl_field ? 2 : 0);
	r = 0;

	for (bytes -= 2 /*PayloadHdr*/; 0 == r && bytes > n; bytes -= len + 2)
	{
		bytes -= n - 2; // skip DON
		ptr += n - 2; // skip DON
		len = nbo_r16(ptr);
		if (len + 2 > bytes)
		{
			assert(0);
			unpacker->flags = RTP_PAYLOAD_FLAG_PACKET_LOST;
			unpacker->size = 0;
			return -EINVAL; // error
		}

		assert(H265_TYPE(ptr[2]) >= 0 && H265_TYPE(ptr[2]) < 48);
		r = unpacker->handler.packet(unpacker->cbparam, ptr + 2, len, timestamp, unpacker->flags);
		unpacker->flags = 0;
		unpacker->size = 0;

		ptr += len + 2; // next NALU
		n = 2 /*LEN*/ + (unpacker->using_donl_field ? 1 : 0);
	}

	return 0 == r ? 1 : r; // packet handled
}

// 4.4.3. Fragmentation Units (p29)
/*
 0               1               2               3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|     PayloadHdr (Type=49)      |    FU header  |  DONL (cond)  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-|
|  DONL (cond)  |                                               |
|-+-+-+-+-+-+-+-+                                               |
|                           FU payload                          |
|                                                               |
|                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                               :    ...OPTIONAL RTP padding    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

+---------------+
|0|1|2|3|4|5|6|7|
+-+-+-+-+-+-+-+-+
|S|E|   FuType  |
+---------------+
*/
static int rtp_h265_unpack_fu(struct rtp_decode_h265_t *unpacker, const uint8_t* ptr, int bytes, uint32_t timestamp)
{
	int r, n;
	uint8_t fuheader;

	r = 0;
	n = 1 /*FU header*/ + (unpacker->using_donl_field ? 4 : 2);
	if (bytes < n || unpacker->size + bytes - n > RTP_PAYLOAD_MAX_SIZE)
	{
		assert(0);
		return -EINVAL;
	}

	if (unpacker->size + bytes - n + 2 /*NALU*/ > unpacker->capacity)
	{
		void* p = NULL;
		int size = unpacker->size + bytes + 2;
		size += size / 4 > 128000 ? size / 4 : 128000;
		p = realloc(unpacker->ptr, size);
		if (!p)
		{
			// set packet lost flag
			unpacker->flags = RTP_PAYLOAD_FLAG_PACKET_LOST;
			unpacker->size = 0;
			return -ENOMEM;
		}
		unpacker->ptr = (uint8_t*)p;
		unpacker->capacity = size;
	}

	fuheader = ptr[2];
	if (FU_START(fuheader))
	{
#if 0
		if (unpacker->size > 0)
		{
			unpacker->flags |= RTP_PAYLOAD_FLAG_PACKET_CORRUPT;
			unpacker->handler.packet(unpacker->cbparam, unpacker->ptr, unpacker->size, unpacker->timestamp, unpacker->flags);
			unpacker->flags = 0;
			unpacker->size = 0; // reset
		}
#endif

		assert(unpacker->capacity > 2);
		unpacker->size = 2; // NAL unit type byte
		unpacker->ptr[0] = (FU_NAL(fuheader) << 1) | (ptr[0] & 0x81); // replace NAL Unit Type Bits
		unpacker->ptr[1] = ptr[1];
		assert(H265_TYPE(unpacker->ptr[0]) >= 0 && H265_TYPE(unpacker->ptr[0]) <= 63);
	}
	else
	{
		if (0 == unpacker->size)
		{
			unpacker->flags = RTP_PAYLOAD_FLAG_PACKET_LOST;
			return 0; // packet discard
		}
		assert(unpacker->size > 0);
	}

	unpacker->timestamp = timestamp;
	if (bytes > n)
	{
		assert(unpacker->capacity >= unpacker->size + bytes - n);
		memmove(unpacker->ptr + unpacker->size, ptr + n, bytes - n);
		unpacker->size += bytes - n;
	}

	if (FU_END(fuheader))
	{
		r = unpacker->handler.packet(unpacker->cbparam, unpacker->ptr, unpacker->size, timestamp, unpacker->flags);
		unpacker->flags = 0;
		unpacker->size = 0;
	}

	return 0 == r ? 1 : r; // packet handled
}

static int rtp_h265_unpack_input(void* p, const void* packet, int bytes)
{
	int r, nal;
	const uint8_t* ptr;
	struct rtp_packet_t pkt;
	struct rtp_decode_h265_t *unpacker;

	unpacker = (struct rtp_decode_h265_t *)p;
	if (!unpacker || 0 != rtp_packet_deserialize(&pkt, packet, bytes) || pkt.payloadlen < (unpacker->using_donl_field ? 5 : 3))
		return -EINVAL;

	if (-1 == unpacker->flags)
	{
		unpacker->flags = 0;
		unpacker->seq = (uint16_t)(pkt.rtp.seq - 1); // disable packet lost
	}

	if ((uint16_t)pkt.rtp.seq != (uint16_t)(unpacker->seq + 1))
	{
		unpacker->flags = RTP_PAYLOAD_FLAG_PACKET_LOST;
		unpacker->size = 0; // discard previous packets
	}
	unpacker->seq = (uint16_t)pkt.rtp.seq;

	assert(pkt.payloadlen > 2);
	ptr = (const uint8_t*)pkt.payload;
	nal = H265_TYPE(ptr[0]);

	if (nal > 50)
		return 0; // packet discard, Unsupported (HEVC) NAL type

	switch (nal)
	{
	case 48: // aggregated packet (AP) - with two or more NAL units
		return rtp_h265_unpack_ap(unpacker, ptr, pkt.payloadlen, pkt.rtp.timestamp);

	case 49: // fragmentation unit (FU)
		return rtp_h265_unpack_fu(unpacker, ptr, pkt.payloadlen, pkt.rtp.timestamp);

	case 50: // TODO: 4.4.4. PACI Packets (p32)
		assert(0);
		return 0; // packet discard

	case 32: // video parameter set (VPS)
	case 33: // sequence parameter set (SPS)
	case 34: // picture parameter set (PPS)
	case 39: // supplemental enhancement information (SEI)
	default: // 4.4.1. Single NAL Unit Packets (p24)
		r = unpacker->handler.packet(unpacker->cbparam, ptr, pkt.payloadlen, pkt.rtp.timestamp, unpacker->flags);
		unpacker->flags = 0;
		unpacker->size = 0;
		return 0 == r ? 1 : r; // packet handled
	}
}

struct rtp_payload_decode_t *rtp_h265_decode()
{
	static struct rtp_payload_decode_t unpacker = {
		rtp_h265_unpack_create,
		rtp_h265_unpack_destroy,
		rtp_h265_unpack_input,
	};

	return &unpacker;
}
