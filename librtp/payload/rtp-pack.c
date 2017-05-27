#include "ctypedef.h"
#include "rtp-pack.h"
#include "rtp-packet.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

struct rtp_packer_t
{
	struct rtp_pack_func_t func;
	void* cbparam;

	struct rtp_packet_t pkt;
	uint32_t frequency;
};

static void* rtp_pack_create(uint8_t pt, uint16_t seq, uint32_t ssrc, uint32_t frequency, struct rtp_pack_func_t *func, void* param)
{
	struct rtp_packer_t *packer;
	packer = (struct rtp_packer_t *)malloc(sizeof(*packer));
	if (!packer) return NULL;

	memset(packer, 0, sizeof(*packer));
	memcpy(&packer->func, func, sizeof(packer->func));
	packer->cbparam = param;
	packer->frequency = frequency;

	packer->pkt.rtp.v = RTP_VERSION;
	packer->pkt.rtp.pt = pt;
	packer->pkt.rtp.seq = seq;
	packer->pkt.rtp.ssrc = ssrc;
	return packer;
}

void rtp_pack_destroy(void* p)
{
	struct rtp_packer_t *packer;
	packer = (struct rtp_packer_t *)p;
#if defined(_DEBUG) || defined(DEBUG)
	memset(packer, 0xCC, sizeof(*packer));
#endif
	free(packer);
}

void rtp_pack_get_info(void* p, uint16_t* seq, uint32_t* timestamp)
{
	struct rtp_packer_t *packer;
	packer = (struct rtp_packer_t *)p;
	*seq = (uint16_t)packer->pkt.rtp.seq;
	*timestamp = packer->pkt.rtp.timestamp;
}

int rtp_pack_input(void* p, const void* data, size_t bytes, int64_t time)
{
	int n;
	uint8_t *rtp;
	const uint8_t *ptr;
	size_t MAX_PACKET;
	struct rtp_packer_t *packer;

	packer = (struct rtp_packer_t *)p;
	packer->pkt.rtp.timestamp = (uint32_t)time * packer->frequency / 1000; // ms -> 8KHZ
	packer->pkt.rtp.m = 0; // marker bit alway 0

	MAX_PACKET = rtp_pack_getsize(); // get packet size
	for(ptr = (const uint8_t *)data; bytes > 0; ++packer->pkt.rtp.seq)
	{
		packer->pkt.payload = ptr;
		packer->pkt.payloadlen = bytes < MAX_PACKET ? bytes : MAX_PACKET;
		ptr += packer->pkt.payloadlen;
		bytes -= packer->pkt.payloadlen;

		n = RTP_FIXED_HEADER + packer->pkt.payloadlen;
		rtp = (uint8_t*)packer->func.alloc(packer->cbparam, n);
		if (!rtp) return ENOMEM;

		n = rtp_packet_serialize(&packer->pkt, rtp, n);
		if ((size_t)n != RTP_FIXED_HEADER + packer->pkt.payloadlen)
		{
			assert(0);
			return -1;
		}

		packer->pkt.rtp.timestamp += packer->pkt.payloadlen * packer->frequency / 1000;

		packer->func.packet(packer->cbparam, rtp, n, time);
		packer->func.free(packer->cbparam, rtp);
	}

	return 0;
}

struct rtp_pack_t *rtp_packer()
{
	static struct rtp_pack_t packer = {
		rtp_pack_create,
		rtp_pack_destroy,
		rtp_pack_get_info,
		rtp_pack_input,
	};

	return &packer;
}
