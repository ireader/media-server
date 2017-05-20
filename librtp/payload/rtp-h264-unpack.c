#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include "ctypedef.h"
#include "rtp-unpack.h"
#include "rtp-packet.h"

#define H264_NAL(v)	(v & 0x1F)
#define FU_START(v) (v & 0x80)
#define FU_END(v)	(v & 0x40)
#define FU_NAL(v)	(v & 0x1F)

struct rtp_h264_unpack_t
{
	struct rtp_unpack_func_t func;
	void* cbparam;

	uint16_t seq; // rtp seq

	uint8_t* ptr;
	int size, capacity;
};

static void* rtp_h264_unpack_create(struct rtp_unpack_func_t *func, void* param)
{
	struct rtp_h264_unpack_t *unpacker;
	unpacker = (struct rtp_h264_unpack_t *)malloc(sizeof(*unpacker));
	if(!unpacker)
		return NULL;

	memset(unpacker, 0, sizeof(*unpacker));
	memcpy(&unpacker->func, func, sizeof(unpacker->func));
	unpacker->cbparam = param;
	return unpacker;
}

static void rtp_h264_unpack_destroy(void* p)
{
	struct rtp_h264_unpack_t *unpacker;
	unpacker = (struct rtp_h264_unpack_t *)p;

	if(unpacker->ptr)
		free(unpacker->ptr);
#if defined(_DEBUG) || defined(DEBUG)
	memset(unpacker, 0xCC, sizeof(*unpacker));
#endif
	free(unpacker);
}

static int rtp_h264_unpack_stap_a(struct rtp_h264_unpack_t *unpacker, const uint8_t* ptr, size_t bytes, int64_t time)
{
	// 5.7.1. Single-Time Aggregation Packet (STAP) (p22)
	uint16_t len;
	
	++ptr; // STAP-A HDR
	for(bytes -= 1; bytes > 2; bytes -= len + 2)
	{
		len = nbo_r16(ptr);
		if((size_t)len + 2 > bytes)
		{
			assert(0);
			return -1; // error
		}

		assert(H264_NAL(ptr[2]) > 0 && H264_NAL(ptr[2]) < 24);
		unpacker->func.packet(unpacker->cbparam, ptr+2, len, time, 0);

		ptr += len + 2; // next NALU
	}

	return 0;
}

static int rtp_h264_unpack_stap_b(struct rtp_h264_unpack_t *unpacker, const uint8_t* ptr, size_t bytes, int64_t time)
{
	// 5.7.1. Single-Time Aggregation Packet (STAP) (p23)
	uint16_t don;
	uint16_t len;

	++ptr; // STAP-B HDR
	don = nbo_r16(ptr);
	ptr += 2;

	for(bytes -= 3; bytes > 2; bytes -= len + 2)
	{
		len = nbo_r16(ptr);
		if((size_t)len + 2 > bytes)
		{
			assert(0);
			return -1; // error
		}

		assert(H264_NAL(ptr[2]) > 0 && H264_NAL(ptr[2]) < 24);
		unpacker->func.packet(unpacker->cbparam, ptr+2, len, time, 0);

		ptr += len + 2; // next NALU
	}

	return 0;
}

static int rtp_h264_unpack_mtap16(struct rtp_h264_unpack_t *unpacker, const uint8_t* ptr, size_t bytes, int64_t time)
{
	// 5.7.2. Multi-Time Aggregation Packets (MTAPs) (p27)
	uint16_t dond;
	uint16_t don;
	uint16_t len;
	uint16_t ts;

	++ptr; // MTAP-16 HDR
	don = nbo_r16(ptr);
	ptr += 2;

	for(bytes -= 3; bytes > 5; bytes -= len + 2)
	{
		len = nbo_r16(ptr);
		if((size_t)len + 2 > bytes || len < 4)
		{
			assert(0);
			return -1; // error
		}

		dond = ptr[2];
		ts = nbo_r16(ptr + 3);
		assert(H264_NAL(ptr[5]) > 0 && H264_NAL(ptr[5]) < 24);
		unpacker->func.packet(unpacker->cbparam, ptr + 5, len - 3, time, 0);

		ptr += len + 2; // next NALU
	}

	return 0;
}

static int rtp_h264_unpack_mtap24(struct rtp_h264_unpack_t *unpacker, const uint8_t* ptr, size_t bytes, int64_t time)
{
	// 5.7.2. Multi-Time Aggregation Packets (MTAPs) (p27)
	uint16_t dond;
	uint16_t don;
	uint16_t len;
	uint16_t ts;

	++ptr; // MTAP-24 HDR
	don = nbo_r16(ptr);
	ptr += 2;

	for(bytes -= 3; bytes > 6; bytes -= len + 2)
	{
		len = nbo_r16(ptr);
		if((size_t)len > bytes - 2 || len < 5)
		{
			assert(0);
			return -1; // error
		}

		dond = ptr[2];
		ts = (ptr[3] << 16) | (ptr[4] << 8) | ptr[5];
		assert(H264_NAL(ptr[6]) > 0 && H264_NAL(ptr[6]) < 24);
		unpacker->func.packet(unpacker->cbparam, ptr + 6, len - 4, time, 0);

		ptr += len + 2; // next NALU
	}

	return 0;
}

static int rtp_h264_unpack_fu_a(struct rtp_h264_unpack_t *unpacker, const uint8_t* ptr, int bytes, int64_t time)
{
	uint8_t fuheader;
	if (bytes < 2)
		return -1;
	
	if(unpacker->size + bytes > unpacker->capacity)
	{
		void* p = NULL;
		int size = unpacker->size + bytes + 128000 + 2;
		p = realloc(unpacker->ptr, size);
		if(!p)
		{
			//unpacker->flags = 1; // lost packet
			unpacker->size = 0;
			return -1;
		}
		unpacker->ptr = (uint8_t*)p;
		unpacker->capacity = size;
	}

	fuheader = ptr[1];
	if (FU_START(fuheader))
	{
		assert(0 == unpacker->size);
		unpacker->size = 1; // NAL unit type byte
		unpacker->ptr[0] = (ptr[0]/*indicator*/ & 0xE0) | (fuheader & 0x1F);
		assert(H264_NAL(unpacker->ptr[0]) > 0 && H264_NAL(unpacker->ptr[0]) < 24);
	}
	else
	{
		assert(0 < unpacker->size);
	}

	memmove(unpacker->ptr + unpacker->size, ptr + 2, bytes - 2);
	unpacker->size += bytes - 2;

	if(FU_END(fuheader))
	{
		unpacker->func.packet(unpacker->cbparam, unpacker->ptr, unpacker->size, time, 0);
		unpacker->size = 0; // reset
	}

	return 0;
}

static int rtp_h264_unpack_fu_b(struct rtp_h264_unpack_t *unpacker, const uint8_t* ptr, int bytes, int64_t time)
{
	uint8_t fuheader;
	uint16_t don;
	if (bytes < 4)
		return -1;
	
	if(unpacker->size + bytes > unpacker->capacity)
	{
		void* p = NULL;
		int size = unpacker->size + bytes + 128000 + 2;
		p = realloc(unpacker->ptr, size);
		if(!p)
		{
			//unpacker->flags = 1; // lost packet
			unpacker->size = 0;
			return -1;
		}
		unpacker->ptr = (uint8_t*)p;
		unpacker->capacity = size;
	}

	fuheader = ptr[1];
	don = nbo_r16(ptr + 2);
	if (FU_START(fuheader))
	{
		assert(0 == unpacker->size);
		unpacker->size = 1; // NAL unit type byte
		unpacker->ptr[0] = (ptr[0]/*indicator*/ & 0xE0) | (fuheader & 0x1F);
		assert(H264_NAL(unpacker->ptr[0]) > 0 && H264_NAL(unpacker->ptr[0]) < 24);
	}
	else
	{
		assert(0 < unpacker->size);
	}

	memmove(unpacker->ptr + unpacker->size, ptr + 4, bytes - 4);
	unpacker->size += bytes - 4;

	if(FU_END(fuheader))
	{
		unpacker->func.packet(unpacker->cbparam, unpacker->ptr, unpacker->size, time, 0);
		unpacker->size = 0; // reset
	}

	return 0;
}

static int rtp_h264_unpack_input(void* p, const void* packet, size_t bytes, uint64_t time)
{
	unsigned char nal;
	struct rtp_packet_t pkt;
	struct rtp_h264_unpack_t *unpacker;

	unpacker = (struct rtp_h264_unpack_t *)p;
	if(!unpacker || 0 != rtp_packet_deserialize(&pkt, packet, bytes) || pkt.payloadlen < 1)
		return -1;

	if((uint16_t)pkt.rtp.seq != unpacker->seq+1)
	{
		// packet lost
		unpacker->size = 0; // clear fu-a/b flags
	}

	unpacker->seq = (uint16_t)pkt.rtp.seq;

	assert(pkt.payloadlen > 0);
	nal = ((unsigned char *)pkt.payload)[0];

	switch(nal & 0x1F)
	{
	case 0: // reserved
	case 31: // reserved
		assert(0);
		break;

	case 24: // STAP-A
		rtp_h264_unpack_stap_a(unpacker, (const uint8_t*)pkt.payload, pkt.payloadlen, time);
		break;
	case 25: // STAP-B
		assert(0);
		//rtp_h264_unpack_stap_b(unpacker, (const uint8_t*)pkt.payload, pkt.payloadlen, time);
		break;
	case 26: // MTAP16
		assert(0);
		//rtp_h264_unpack_mtap16(unpacker, (const uint8_t*)pkt.payload, pkt.payloadlen, time);
		break;
	case 27: // MTAP24
		assert(0);
		//rtp_h264_unpack_mtap24(unpacker, (const uint8_t*)pkt.payload, pkt.payloadlen, time);
		break;
	case 28: // FU-A
		rtp_h264_unpack_fu_a(unpacker, (const uint8_t*)pkt.payload, pkt.payloadlen, time);
		break;
	case 29: // FU-B
		rtp_h264_unpack_fu_b(unpacker, (const uint8_t*)pkt.payload, pkt.payloadlen, time);
		break;

	default: // 1-23 NAL unit
		unpacker->func.packet(unpacker->cbparam, (const uint8_t*)pkt.payload, pkt.payloadlen, time, 0);
		break;
	}

	return 1;
}

struct rtp_unpack_t *rtp_h264_unpacker()
{
	static struct rtp_unpack_t unpacker = {
		rtp_h264_unpack_create,
		rtp_h264_unpack_destroy,
		rtp_h264_unpack_input,
	};

	return &unpacker;
}
