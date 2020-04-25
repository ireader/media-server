/// RFC3555 MIME Type Registration of RTP Payload Formats
/// 4.2.11 Registration of MIME media type video/MP2P (p40)
/// 
/// RFC2250 2. Encapsulation of MPEG System and Transport Streams (p3)
/// 1. Each RTP packet will contain a timestamp derived from the sender's 90KHz clock reference
/// 2. For MPEG2 Program streams and MPEG1 system streams there are no packetization restrictions; 
///    these streams are treated as a packetized stream of bytes.
///
/// 2.1 RTP header usage (p4)
/// 32 bit 90K Hz timestamp representing the target transmission time for the first byte of the packet

#include "rtp-packet.h"
#include "rtp-profile.h"
#include "rtp-payload-internal.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#define TS_PACKET_SIZE 188

#define KHz 90 // 90000Hz

struct rtp_encode_ts_t
{
	struct rtp_packet_t pkt;
	struct rtp_payload_t handler;
	void* cbparam;
	int size;
};

static void* rtp_ts_pack_create(int size, uint8_t pt, uint16_t seq, uint32_t ssrc, struct rtp_payload_t *handler, void* cbparam)
{
	struct rtp_encode_ts_t *packer;
	packer = (struct rtp_encode_ts_t *)calloc(1, sizeof(*packer));
	if (!packer) return NULL;

	//assert(pt == RTP_PAYLOAD_MP2T);
	if (RTP_PAYLOAD_MP2T == pt)
	{
		size -= RTP_FIXED_HEADER;
		size = size / TS_PACKET_SIZE * TS_PACKET_SIZE;
		size += RTP_FIXED_HEADER;
		if (size < 64)
		{
			free(packer);
			return NULL;
		}
	}

	memcpy(&packer->handler, handler, sizeof(packer->handler));
	packer->cbparam = cbparam;
	packer->size = size;

	packer->pkt.rtp.v = RTP_VERSION;
	packer->pkt.rtp.pt = pt;
	packer->pkt.rtp.seq = seq;
	packer->pkt.rtp.ssrc = ssrc;
	return packer;
}

static void rtp_ts_pack_destroy(void* pack)
{
	struct rtp_encode_ts_t *packer;
	packer = (struct rtp_encode_ts_t *)pack;
#if defined(_DEBUG) || defined(DEBUG)
	memset(packer, 0xCC, sizeof(*packer));
#endif
	free(packer);
}

static void rtp_ts_pack_get_info(void* pack, uint16_t* seq, uint32_t* timestamp)
{
	struct rtp_encode_ts_t *packer;
	packer = (struct rtp_encode_ts_t *)pack;
	*seq = (uint16_t)packer->pkt.rtp.seq;
	*timestamp = packer->pkt.rtp.timestamp;
}

static int rtp_ts_pack_input(void* pack, const void* data, int bytes, uint32_t timestamp)
{
	int n;
	uint8_t *rtp;
	const uint8_t *ptr;
	struct rtp_encode_ts_t *packer;
	packer = (struct rtp_encode_ts_t *)pack;
	packer->pkt.rtp.timestamp = timestamp; //(uint32_t)(time * KHz); // ms -> 90KHZ (RFC2250 section2 p2)

	for (ptr = (const uint8_t *)data; bytes > 0; ++packer->pkt.rtp.seq)
	{
		packer->pkt.payload = ptr;
		packer->pkt.payloadlen = (bytes + RTP_FIXED_HEADER) <= packer->size ? bytes : (packer->size - RTP_FIXED_HEADER);
		ptr += packer->pkt.payloadlen;
		bytes -= packer->pkt.payloadlen;

		n = RTP_FIXED_HEADER + packer->pkt.payloadlen;
		rtp = (uint8_t*)packer->handler.alloc(packer->cbparam, n);
		if (!rtp) return ENOMEM;

		// M bit: Set to 1 whenever the timestamp is discontinuous
		//packer->pkt.rtp.m = (bytes <= packer->size) ? 1 : 0;
		packer->pkt.rtp.m = 0;
		n = rtp_packet_serialize(&packer->pkt, rtp, n);
		if (n != RTP_FIXED_HEADER + packer->pkt.payloadlen)
		{
			assert(0);
			return -1;
		}

		packer->handler.packet(packer->cbparam, rtp, n, packer->pkt.rtp.timestamp, 0);
		packer->handler.free(packer->cbparam, rtp);
	}

	return 0;
}

struct rtp_payload_encode_t *rtp_ts_encode()
{
	static struct rtp_payload_encode_t encode = {
		rtp_ts_pack_create,
		rtp_ts_pack_destroy,
		rtp_ts_pack_get_info,
		rtp_ts_pack_input,
	};

	return &encode;
}
