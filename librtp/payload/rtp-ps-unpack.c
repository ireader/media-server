#include <stdlib.h>
#include <memory.h>
#include <assert.h>
#include <errno.h>
#include "cstringext.h"
#include "ctypedef.h"
#include "rtp-unpack.h"
#include "rtp-packet.h"

struct rtp_ps_unpack_t
{
	struct rtp_unpack_func_t func;
	void* cbparam;

	uint16_t seq; // rtp seq
	int flag; // lost packet

	uint8_t* ptr;
	size_t size, capacity;
};

static void* rtp_ps_unpack_create(struct rtp_unpack_func_t *func, void* param)
{
	struct rtp_ps_unpack_t *unpacker;
	unpacker = (struct rtp_ps_unpack_t *)malloc(sizeof(*unpacker));
	if(!unpacker)
		return NULL;

	memset(unpacker, 0, sizeof(*unpacker));
	memcpy(&unpacker->func, func, sizeof(unpacker->func));
	unpacker->cbparam = param;
	return unpacker;
}

static void rtp_ps_unpack_destroy(void* p)
{
	struct rtp_ps_unpack_t *unpacker;
	unpacker = (struct rtp_ps_unpack_t *)p;

	if(unpacker->ptr)
		free(unpacker->ptr);
#if defined(_DEBUG) || defined(DEBUG)
	memset(unpacker, 0xCC, sizeof(*unpacker));
#endif
	free(unpacker);
}

static int rtp_ps_unpack_input(void* p, const void* packet, size_t bytes, int64_t time)
{
	rtp_packet_t pkt;
	struct rtp_ps_unpack_t *unpacker;

	unpacker = (struct rtp_ps_unpack_t *)p;
	if(0 != rtp_packet_deserialize(&pkt, packet, bytes) || pkt.payloadlen < 1)
		return -1;

	if((uint16_t)pkt.rtp.seq != unpacker->seq+1 && 0!=unpacker->seq)
	{
		// packet lost
		unpacker->size = 0; // clear flags
		unpacker->flag = 1;
	}

	unpacker->seq = (uint16_t)pkt.rtp.seq;

	assert(pkt.payloadlen > 0);
	if(pkt.payloadlen > 0 && 0 == unpacker->flag)
	{
		if(pkt.payloadlen > 0 && unpacker->size + pkt.payloadlen > unpacker->capacity)
		{
			uint8_t *ptr = (uint8_t*)realloc(unpacker->ptr, unpacker->capacity + pkt.payloadlen + 2048);
			if(!p)
			{
				unpacker->flag = 1;
				return ENOMEM;
			}

			unpacker->ptr = ptr;
			unpacker->capacity += pkt.payloadlen + 2048;
		}

		memcpy(unpacker->ptr + unpacker->size, pkt.payload, pkt.payloadlen);
		unpacker->size += pkt.payloadlen;
	}

	// RTP marker bit
	if(pkt.rtp.m)
	{
		if(unpacker->size > 0 && 0==unpacker->flag)
			unpacker->func.packet(unpacker->cbparam, (uint8_t)pkt.rtp.pt, unpacker->ptr, unpacker->size, time);
		unpacker->flag = 0;
		unpacker->size = 0;
	}

	return 0;
}

struct rtp_unpack_t *rtp_ps_unpacker()
{
	static struct rtp_unpack_t unpacker = {
		rtp_ps_unpack_create,
		rtp_ps_unpack_destroy,
		rtp_ps_unpack_input,
	};

	return &unpacker;
}
