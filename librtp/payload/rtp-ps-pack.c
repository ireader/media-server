/// RFC3555 MIME Type Registration of RTP Payload Formats
/// 4.2.11 Registration of MIME media type video/MP2P (p40)
/// 
/// RFC2250 2. Encapsulation of MPEG System and Transport Streams (p3)
/// 1. Each RTP packet will contain a timestamp derived from the sender¡¯s 90KHz clock reference
/// 2. For MPEG2 Program streams and MPEG1 system streams there are no packetization restrictions; 
///    these streams are treated as a packetized stream of bytes.

#include "ctypedef.h"
#include "rtp-pack.h"
#include "rtp-packet.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#define KHz 90 // 90000Hz

struct rtp_ps_packer_t
{
	struct rtp_packet_t pkt;
	struct rtp_pack_func_t func;
	void* cbparam;
};

static void* rtp_ps_pack_create(uint8_t pt, uint16_t seq, uint32_t ssrc, uint32_t frequency, struct rtp_pack_func_t *func, void* param)
{
	struct rtp_ps_packer_t *packer;
	packer = (struct rtp_ps_packer_t *)malloc(sizeof(*packer));
	if(!packer) return NULL;

	memset(packer, 0, sizeof(*packer));
	memcpy(&packer->func, func, sizeof(packer->func));
	packer->cbparam = param;

	assert(KHz * 1000 == frequency);
	packer->pkt.rtp.v = RTP_VERSION;
	packer->pkt.rtp.pt = pt;
	packer->pkt.rtp.seq = seq;
	packer->pkt.rtp.ssrc = ssrc;
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

static void rtp_ps_pack_get_info(void* pack, uint16_t* seq, uint32_t* timestamp)
{
	struct rtp_ps_packer_t *packer;
	packer = (struct rtp_ps_packer_t *)pack;
	*seq = (uint16_t)packer->pkt.rtp.seq;
	*timestamp = packer->pkt.rtp.timestamp;
}

static int rtp_ps_pack_input(void* pack, const void* ps, size_t bytes, int64_t time)
{
	int n;
	uint8_t *rtp;
	const uint8_t *ptr;
	size_t MAX_PACKET;
	struct rtp_ps_packer_t *packer;
	packer = (struct rtp_ps_packer_t *)pack;
	packer->pkt.rtp.timestamp = (uint32_t)(time * KHz); // ms -> 90KHZ (RFC2250 section2 p2)

	MAX_PACKET = rtp_pack_getsize(); // get packet size
	for (ptr = (const uint8_t *)ps; bytes > 0; ++packer->pkt.rtp.seq)
	{
		packer->pkt.payload = ptr;
		packer->pkt.payloadlen = bytes < MAX_PACKET ? bytes : MAX_PACKET;
		ptr += packer->pkt.payloadlen;
		bytes -= packer->pkt.payloadlen;

		n = RTP_FIXED_HEADER + packer->pkt.payloadlen;
		rtp = (uint8_t*)packer->func.alloc(packer->cbparam, n);
		if (!rtp) return ENOMEM;

		packer->pkt.rtp.m = (bytes <= MAX_PACKET) ? 1 : 0; // set marker flag
		n = rtp_packet_serialize(&packer->pkt, rtp, n);
		if ((size_t)n != RTP_FIXED_HEADER + packer->pkt.payloadlen)
		{
			assert(0);
			return -1;
		}

		packer->func.packet(packer->cbparam, rtp, n, time);
		packer->func.free(packer->cbparam, rtp);
	}

	return 0;
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
