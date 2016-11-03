#include <stdlib.h>
#include <memory.h>
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

	void* ptr;
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

static int rtp_h264_unpack_stap_a(struct rtp_h264_unpack_t *unpacker, const void* data, size_t bytes, uint64_t time)
{
	// 5.7.1. Single-Time Aggregation Packet (STAP) (p20)
	unsigned char stapnal;
	uint16_t nallen;
	const unsigned char* ptr;
	ptr = (const unsigned char*)data;
	stapnal = ptr[0];

	++ptr;
	nallen = 0;
	for(bytes -= 1; bytes > 2; bytes -= nallen + 2)
	{
		unsigned char nal;

		nallen = nbo_r16(ptr);
		if(nallen + 2 > bytes)
		{
			assert(0);
			return -1; // error
		}

		nal = ptr[2];
		assert(H264_NAL(nal) > 0 && H264_NAL(nal) < 24);

		unpacker->func.packet(unpacker->cbparam, nal, ptr+3, nallen-1, time);

		ptr = ptr + nallen + 2; // next NALU
	}

	return 0;
}

static int rtp_h264_unpack_stap_b(struct rtp_h264_unpack_t *unpacker, const void* data, size_t bytes, uint64_t time)
{
	// 5.7.1. Single-Time Aggregation Packet (STAP)
	unsigned char stapnal;
	uint16_t don;
	uint16_t nallen;
	const unsigned char* ptr;
	ptr = (const unsigned char*)data;

	stapnal = ptr[0];
	don = nbo_r16(ptr + 1);

	ptr += 3;
	nallen = 0;
	for(bytes -= 1; bytes > 2; bytes -= nallen + 2)
	{
		unsigned char nal;

		nallen = nbo_r16(ptr);
		if(nallen + 2> bytes)
		{
			assert(0);
			return -1; // error
		}

		nal = ptr[2];
		assert(H264_NAL(nal) > 0 && H264_NAL(nal) < 24);
		unpacker->func.packet(unpacker->cbparam, nal, ptr+3, nallen-1, time);

		ptr = ptr + nallen + 2; // next NALU
	}

	return 0;
}

static int rtp_h264_unpack_mtap16(struct rtp_h264_unpack_t *unpacker, const void* data, int bytes, uint64_t time)
{
	// 5.7.2. Multi-Time Aggregation Packets (MTAPs)
	unsigned char mtapnal;
	uint16_t donb;
	uint16_t nallen;
	const unsigned char* ptr;
	ptr = (const unsigned char*)data;

	mtapnal = ptr[0];
	donb = nbo_r16(ptr+1);

	ptr += 3;
	for(bytes -= 1; bytes > 6; bytes -= nallen + 2)
	{
		unsigned char nal, dond;
		uint16_t timestamp;

		nallen = nbo_r16(ptr);
		if(nallen + 2 > bytes)
		{
			assert(0);
			return -1; // error
		}

		assert(nallen > 4);
		dond = ptr[2];
		timestamp = nbo_r16(ptr+3);

		nal = ptr[5];
		assert(H264_NAL(nal) > 0 && H264_NAL(nal) < 24);
		unpacker->func.packet(unpacker->cbparam, nal, ptr+6, nallen-4, time);

		ptr = ptr + nallen + 2; // next NALU
	}

	return 0;
}

static int rtp_h264_unpack_mtap24(struct rtp_h264_unpack_t *unpacker, const void* data, int bytes, uint64_t time)
{
	// 5.7.2. Multi-Time Aggregation Packets (MTAPs)
	unsigned char mtapnal;
	uint16_t donb;
	uint16_t nallen;
	const unsigned char* ptr;
	ptr = (const unsigned char*)data;

	mtapnal = ptr[0];
	donb = nbo_r16(ptr + 1);

	ptr += 3;
	for(bytes -= 1; bytes > 2; bytes -= nallen + 2)
	{
		unsigned char nal, dond;
		unsigned int timestamp;
		const unsigned short *p;
		p = (const unsigned short *)ptr;

		nallen = nbo_r16(ptr);
		if(nallen > bytes - 2)
		{
			assert(0);
			return -1; // error
		}

		assert(nallen > 5);
		ptr = (const unsigned char*)(p + 1);
		dond = ptr[0];
		timestamp = (ptr[1] << 16) | (ptr[2] << 8) | ptr[3];

		nal = ptr[4];
		assert(H264_NAL(nal) > 0 && H264_NAL(nal) < 24);
		unpacker->func.packet(unpacker->cbparam, nal, ptr+5, nallen-5, time);

		ptr = ptr + nallen; // next NALU
	}

	return 0;
}

static int rtp_h264_unpack_fu_a(struct rtp_h264_unpack_t *unpacker, const void* data, int bytes, uint64_t time)
{
	unsigned char fuindicator, fuheader;
	const unsigned char* ptr;
	ptr = (const unsigned char*)data;

	fuindicator = ptr[0];
	fuheader = ptr[1];

	if(FU_START(fuheader))
	{
		assert(0 == unpacker->size);
		unpacker->size = 0;
	}
	else
	{
		assert(0 < unpacker->size);
	}

	if(unpacker->size + bytes - 2 > unpacker->capacity)
	{
		void* p = NULL;
		int size = unpacker->size + bytes * 2;
		p = realloc(unpacker->ptr, size);
		if(!p)
		{
			unpacker->size = 0;
			return -1;
		}
		unpacker->ptr = p;
		unpacker->capacity = size;
	}

	memmove((char*)unpacker->ptr + unpacker->size, ptr + 2, bytes - 2);
	unpacker->size += bytes - 2;

	if(FU_END(fuheader))
	{
		unsigned char nal;
		nal = (fuindicator & 0xE0) | (fuheader & 0x1F);
		assert(H264_NAL(nal) > 0 && H264_NAL(nal) < 24);
		unpacker->func.packet(unpacker->cbparam, nal, unpacker->ptr, unpacker->size, time);
		unpacker->size = 0; // reset
	}

	return 0;
}

static int rtp_h264_unpack_fu_b(struct rtp_h264_unpack_t *unpacker, const void* data, int bytes, uint64_t time)
{
	unsigned char fuindicator, fuheader;
	unsigned short don;
	const unsigned char* ptr;
	ptr = (const unsigned char*)data;

	fuindicator = ptr[0];
	fuheader = ptr[1];
	don = nbo_r16(ptr + 2);

	if(FU_START(fuheader))
	{
		assert(0 == unpacker->size);
		unpacker->size = 0;
	}
	else
	{
		assert(0 < unpacker->size);
	}

	if(unpacker->size + bytes - 4 > unpacker->capacity)
	{
		void* p = NULL;
		int size = unpacker->size + bytes * 2;
		p = realloc(unpacker->ptr, size);
		if(!p)
		{
			unpacker->size = 0;
			return -1;
		}
		unpacker->ptr = p;
		unpacker->capacity = size;
	}

	memmove((char*)unpacker->ptr + unpacker->size, ptr + 4, bytes - 4);
	unpacker->size += bytes - 4;

	if(FU_END(fuheader))
	{
		unsigned char nal;
		nal = (fuindicator & 0xE0) | (fuheader & 0x1F);
		assert(H264_NAL(nal) > 0 && H264_NAL(nal) < 24);
		unpacker->func.packet(unpacker->cbparam, nal, unpacker->ptr, unpacker->size, time);
		unpacker->size = 0; // reset
	}

	return 0;
}

static int rtp_h264_unpack_input(void* p, const void* packet, size_t bytes, uint64_t time)
{
	rtp_packet_t pkt;
	unsigned char nal;
	struct rtp_h264_unpack_t *unpacker;

	unpacker = (struct rtp_h264_unpack_t *)p;
	if(0 != rtp_packet_deserialize(&pkt, packet, bytes) || pkt.payloadlen < 1)
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
		rtp_h264_unpack_stap_a(unpacker, pkt.payload, pkt.payloadlen, time);
		break;
	case 25: // STAP-B
		assert(0);
		//rtp_h264_unpack_stap_b(unpacker, pkt.payload, pkt.payloadlen, time);
		break;
	case 26: // MTAP16
		assert(0);
		//rtp_h264_unpack_mtap16(unpacker, pkt.payload, pkt.payloadlen, time);
		break;
	case 27: // MTAP24
		assert(0);
		//rtp_h264_unpack_mtap24(unpacker, pkt.payload, pkt.payloadlen, time);
		break;
	case 28: // FU-A
		rtp_h264_unpack_fu_a(unpacker, pkt.payload, pkt.payloadlen, time);
		break;
	case 29: // FU-B
		assert(0);
		//rtp_h264_unpack_fu_b(unpacker, pkt.payload, pkt.payloadlen, time);
		break;

	default: // 1-23 NAL unit
		unpacker->func.packet(unpacker->cbparam, nal, (const unsigned char*)pkt.payload + 1, pkt.payloadlen-1, time);
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
