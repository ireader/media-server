// RFC6184 RTP Payload Format for H.264 Video

#include "rtp-packet.h"
#include "rtp-payload-internal.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#define H264_NAL(v)	(v & 0x1F)
#define FU_START(v) (v & 0x80)
#define FU_END(v)	(v & 0x40)
#define FU_NAL(v)	(v & 0x1F)

struct rtp_decode_h264_t
{
	struct rtp_payload_t handler;
	void* cbparam;

	uint16_t seq; // rtp seq

	uint8_t* ptr;
	int size, capacity;

	int flags;
};

static void* rtp_h264_unpack_create(struct rtp_payload_t *handler, void* param)
{
	struct rtp_decode_h264_t *unpacker;
	unpacker = (struct rtp_decode_h264_t *)calloc(1, sizeof(*unpacker));
	if(!unpacker)
		return NULL;

	memcpy(&unpacker->handler, handler, sizeof(unpacker->handler));
	unpacker->cbparam = param;
	unpacker->flags = -1;
	return unpacker;
}

static void rtp_h264_unpack_destroy(void* p)
{
	struct rtp_decode_h264_t *unpacker;
	unpacker = (struct rtp_decode_h264_t *)p;

	if(unpacker->ptr)
		free(unpacker->ptr);
#if defined(_DEBUG) || defined(DEBUG)
	memset(unpacker, 0xCC, sizeof(*unpacker));
#endif
	free(unpacker);
}

// 5.7.1. Single-Time Aggregation Packet (STAP) (p23)
/*
 0               1               2               3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                           RTP Header                          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|STAP-B NAL HDR |            DON                |  NALU 1 Size  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| NALU 1 Size   | NALU 1 HDR    |         NALU 1 Data           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               +
:                                                               :
+               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|               | NALU 2 Size                   |   NALU 2 HDR  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                            NALU 2 Data                        |
:                                                               :
|                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                               :    ...OPTIONAL RTP padding    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
static int rtp_h264_unpack_stap(struct rtp_decode_h264_t *unpacker, const uint8_t* ptr, int bytes, uint32_t timestamp, int stap_b)
{
	int n;
	uint16_t len;
	uint16_t don;

	n = stap_b ? 3 : 1;
	don = stap_b ? nbo_r16(ptr + 1) : 0;
	ptr += n; // STAP-A / STAP-B HDR + DON

	for(bytes -= n; bytes > 2; bytes -= len + 2)
	{
		len = nbo_r16(ptr);
		if(len + 2 > bytes)
		{
			assert(0);
			unpacker->flags = RTP_PAYLOAD_FLAG_PACKET_LOST;
			unpacker->size = 0;
			return -EINVAL; // error
		}

		assert(H264_NAL(ptr[2]) > 0 && H264_NAL(ptr[2]) < 24);
		unpacker->handler.packet(unpacker->cbparam, ptr + 2, len, timestamp, unpacker->flags);
		unpacker->flags = 0;
		unpacker->size = 0;

		ptr += len + 2; // next NALU
		don = (don + 1) % 65536;
	}

	return 1; // packet handled
}

// 5.7.2. Multi-Time Aggregation Packets (MTAPs) (p27)
/*
 0               1               2               3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                          RTP Header                           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|MTAP16 NAL HDR |   decoding order number base  |  NALU 1 Size  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| NALU 1 Size   | NALU 1 DOND   |         NALU 1 TS offset      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| NALU 1 HDR    |                NALU 1 DATA                    |
+-+-+-+-+-+-+-+-+                                               +
:                                                               :
+               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|               | NALU 2 SIZE                   |   NALU 2 DOND |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| NALU 2 TS offset              | NALU 2 HDR    |  NALU 2 DATA  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+               |
:                                                               :
|                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                               :    ...OPTIONAL RTP padding    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
static int rtp_h264_unpack_mtap(struct rtp_decode_h264_t *unpacker, const uint8_t* ptr, int bytes, uint32_t timestamp, int n)
{
	uint16_t dond;
	uint16_t donb;
	uint16_t len;
	uint32_t ts;

	donb = nbo_r16(ptr + 1);
	ptr += 3; // MTAP16/MTAP24 HDR + DONB

	for(bytes -= 3; bytes > 3 + n; bytes -= len + 2)
	{
		len = nbo_r16(ptr);
		if(len + 2 > bytes || len < 1 /*DOND*/ + n /*TS offset*/ + 1 /*NALU*/)
		{
			assert(0);
			unpacker->flags = RTP_PAYLOAD_FLAG_PACKET_LOST;
			unpacker->size = 0;
			return -EINVAL; // error
		}

		dond = (ptr[2] + donb) % 65536;
		ts = (uint16_t)nbo_r16(ptr + 3);
		if (3 == n) ts = (ts << 16) | ptr[5]; // MTAP24

		// if the NALU-time is larger than or equal to the RTP timestamp of the packet, 
		// then the timestamp offset equals (the NALU - time of the NAL unit - the RTP timestamp of the packet).
		// If the NALU - time is smaller than the RTP timestamp of the packet,
		// then the timestamp offset is equal to the NALU - time + (2 ^ 32 - the RTP timestamp of the packet).
		ts += timestamp; // wrap 1 << 32

		assert(H264_NAL(ptr[n + 3]) > 0 && H264_NAL(ptr[n + 3]) < 24);
		unpacker->handler.packet(unpacker->cbparam, ptr + 1 + n, len - 1 - n, ts, unpacker->flags);
		unpacker->flags = 0;
		unpacker->size = 0;

		ptr += len + 1 + n; // next NALU
	}

	return 1; // packet handled
}

// 5.8. Fragmentation Units (FUs) (p29)
/*
 0               1               2               3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  FU indicator |   FU header   |              DON              |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-|
|                                                               |
|                          FU payload                           |
|                                                               |
|                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                               :   ...OPTIONAL RTP padding     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
static int rtp_h264_unpack_fu(struct rtp_decode_h264_t *unpacker, const uint8_t* ptr, int bytes, uint32_t timestamp, int fu_b)
{
	int n;
	uint8_t fuheader;
	//uint16_t don;

	n = fu_b ? 4 : 2;
	if (bytes < n)
		return -EINVAL; // error

	if (unpacker->size + bytes - n + 1 /*NALU*/ > unpacker->capacity)
	{
		void* p = NULL;
		int size = unpacker->size + bytes + 128000 + 1;
		p = realloc(unpacker->ptr, size);
		if (!p)
		{
			// set packet lost flag
			unpacker->flags = RTP_PAYLOAD_FLAG_PACKET_LOST;
			unpacker->size = 0;
			return -ENOMEM; // error
		}
		unpacker->ptr = (uint8_t*)p;
		unpacker->capacity = size;
	}

	fuheader = ptr[1];
	//don = nbo_r16(ptr + 2);
	if (FU_START(fuheader))
	{
		assert(0 == unpacker->size);
		unpacker->size = 1; // NAL unit type byte
		unpacker->ptr[0] = (ptr[0]/*indicator*/ & 0xE0) | (fuheader & 0x1F);
		assert(H264_NAL(unpacker->ptr[0]) > 0 && H264_NAL(unpacker->ptr[0]) < 24);
	}
	else
	{
		if (0 == unpacker->size)
		{
			assert(0);
			unpacker->flags = RTP_PAYLOAD_FLAG_PACKET_LOST;
			return 0; // packet discard
		}
		assert(unpacker->size > 0);
	}

	if (bytes > n)
	{
		assert(unpacker->capacity >= unpacker->size + bytes - n);
		memmove(unpacker->ptr + unpacker->size, ptr + n, bytes - n);
		unpacker->size += bytes - n;
	}

	if(FU_END(fuheader))
	{
		unpacker->handler.packet(unpacker->cbparam, unpacker->ptr, unpacker->size, timestamp, unpacker->flags);
		unpacker->flags = 0;
		unpacker->size = 0; // reset
	}

	return 1; // packet handled
}

static int rtp_h264_unpack_input(void* p, const void* packet, int bytes)
{
	unsigned char nal;
	struct rtp_packet_t pkt;
	struct rtp_decode_h264_t *unpacker;

	unpacker = (struct rtp_decode_h264_t *)p;
	if(!unpacker || 0 != rtp_packet_deserialize(&pkt, packet, bytes) || pkt.payloadlen < 1)
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

	assert(pkt.payloadlen > 0);
	nal = ((unsigned char *)pkt.payload)[0];

	switch(nal & 0x1F)
	{
	case 0: // reserved
	case 31: // reserved
		assert(0);
		return 0; // packet discard

	case 24: // STAP-A
		return rtp_h264_unpack_stap(unpacker, (const uint8_t*)pkt.payload, pkt.payloadlen, pkt.rtp.timestamp, 0);
	case 25: // STAP-B
		return rtp_h264_unpack_stap(unpacker, (const uint8_t*)pkt.payload, pkt.payloadlen, pkt.rtp.timestamp, 1);
	case 26: // MTAP16
		return rtp_h264_unpack_mtap(unpacker, (const uint8_t*)pkt.payload, pkt.payloadlen, pkt.rtp.timestamp, 2);
	case 27: // MTAP24
		return rtp_h264_unpack_mtap(unpacker, (const uint8_t*)pkt.payload, pkt.payloadlen, pkt.rtp.timestamp, 3);
	case 28: // FU-A
		return rtp_h264_unpack_fu(unpacker, (const uint8_t*)pkt.payload, pkt.payloadlen, pkt.rtp.timestamp, 0);
	case 29: // FU-B
		return rtp_h264_unpack_fu(unpacker, (const uint8_t*)pkt.payload, pkt.payloadlen, pkt.rtp.timestamp, 1);

	default: // 1-23 NAL unit
		unpacker->handler.packet(unpacker->cbparam, (const uint8_t*)pkt.payload, pkt.payloadlen, pkt.rtp.timestamp, unpacker->flags);
		unpacker->flags = 0;
		unpacker->size = 0;
		return 1; // packet handled
	}
}

struct rtp_payload_decode_t *rtp_h264_decode()
{
	static struct rtp_payload_decode_t unpacker = {
		rtp_h264_unpack_create,
		rtp_h264_unpack_destroy,
		rtp_h264_unpack_input,
	};

	return &unpacker;
}
