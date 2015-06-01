/// RFC3555 MIME Type Registration of RTP Payload Formats
/// 4.2.11 Registration of MIME media type video/MP2P (p40)
/// 
/// RFC2250 2. Encapsulation of MPEG System and Transport Streams (p3)
/// 1. Each RTP packet will contain a timestamp derived from the sender¡¯s 90KHz clock reference
/// 2. For MPEG2 Program streams and MPEG1 system streams there are no packetization restrictions; 
///    these streams are treated as a packetized stream of bytes.

#include <stdlib.h>
#include <memory.h>
#include <assert.h>
#include <errno.h>
#include "ctypedef.h"
#include "rtp-pack.h"

struct rtp_ps_packer_t
{
	uint32_t ssrc;
	uint32_t timestamp;
	uint16_t seq;
	uint8_t payload;

	struct rtp_pack_func_t func;
	void* cbparam;
};

static void* rtp_ps_pack_create(uint32_t ssrc, unsigned short seq, uint8_t payload, struct rtp_pack_func_t *func, void* param)
{
	struct rtp_ps_packer_t *packer;
	packer = (struct rtp_ps_packer_t *)malloc(sizeof(*packer));
	if(!packer) return NULL;

	memset(packer, 0, sizeof(*packer));
	memcpy(&packer->func, func, sizeof(packer->func));
	packer->cbparam = param;
	packer->ssrc = ssrc;
	packer->payload = payload;
	packer->seq = seq;
	packer->timestamp = 0;
	return packer;
}

static void rtp_ps_pack_destroy(void* pack)
{
	struct rtp_ps_packer_t *packer;
	packer = (struct rtp_ps_packer_t *)pack;
#if defined(_DEBUG) || defined(DEBUG)
	memset(packer, 0xCC, sizeof(*packer));
#endif
	free(packer);
}

static unsigned char* alloc_packet(struct rtp_ps_packer_t *packer, uint32_t timestamp, size_t bytes)
{
	unsigned char *rtp;
	rtp = (unsigned char*)packer->func.alloc(packer->cbparam, bytes+14);
	if(!rtp)
		return NULL;

	rtp[0] = (unsigned char)(0x80);
	rtp[1] = (unsigned char)(packer->payload);

	rtp[4] = (unsigned char)(timestamp >> 24);
	rtp[5] = (unsigned char)(timestamp >> 16);
	rtp[6] = (unsigned char)(timestamp >> 8);
	rtp[7] = (unsigned char)(timestamp);

	rtp[8] = (unsigned char)(packer->ssrc >> 24);
	rtp[9] = (unsigned char)(packer->ssrc >> 16);
	rtp[10] = (unsigned char)(packer->ssrc >> 8);
	rtp[11] = (unsigned char)(packer->ssrc);

	return rtp;
}

static int rtp_ps_pack_input(void* pack, const void* ps, size_t bytes, uint64_t time)
{
	size_t MAX_PACKET;
	const unsigned char *p;
	struct rtp_ps_packer_t *packer;
	packer = (struct rtp_ps_packer_t *)pack;

	packer->timestamp = (uint32_t)time * 90; // ms -> 90KHZ (RFC2250 section2 p2)

	MAX_PACKET = rtp_pack_getsize(); // get packet size

	p = (const unsigned char *)ps;
	while(bytes > 0)
	{
		size_t len;
		unsigned char *rtp;

		rtp = alloc_packet(packer, packer->timestamp, MAX_PACKET);
		if(!rtp) return ENOMEM;
		assert(0 == (rtp[1] & 0x80)); // don't set market
		if(bytes <= MAX_PACKET)
			rtp[1] |= 0x80; // set marker flag
		rtp[2] = (unsigned char)(packer->seq >> 8);
		rtp[3] = (unsigned char)(packer->seq);
		++packer->seq;

		len = bytes > MAX_PACKET ? MAX_PACKET : bytes;
		memcpy(rtp+12, p, len);

		packer->func.packet(packer->cbparam, rtp, len+12, time);
		packer->func.free(packer->cbparam, rtp);
		p += len;
		bytes -= len;
	}
	return 0;
}

static void rtp_ps_pack_get_info(void* pack, unsigned short* seq, unsigned int* timestamp)
{
	struct rtp_ps_packer_t *packer;
	packer = (struct rtp_ps_packer_t *)pack;
	*seq = packer->seq;
	*timestamp = packer->timestamp;
}

struct rtp_pack_t *rtp_ps_packer()
{
	static struct rtp_pack_t packer = {
		rtp_ps_pack_create,
		rtp_ps_pack_destroy,
		rtp_ps_pack_get_info,
		rtp_ps_pack_input,
	};

	return &packer;
}
