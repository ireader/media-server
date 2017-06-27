// RFC7731 RTP Payload Format for VP8 Video
//

#include "rtp-packet.h"
#include "rtp-profile.h"
#include "rtp-payload-internal.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

// Timestamp: The granularity of the clock is 90 kHz
#define KHz 90 // 90000Hz

#define N_VP8_HEADER 1

struct rtp_encode_vp8_t
{
	struct rtp_packet_t pkt;
	struct rtp_payload_t handler;
	void* cbparam;
	int size;
};

static void* rtp_vp8_pack_create(int size, uint8_t pt, uint16_t seq, uint32_t ssrc, struct rtp_payload_t *handler, void* cbparam)
{
	struct rtp_encode_vp8_t *packer;
	packer = (struct rtp_encode_vp8_t *)calloc(1, sizeof(*packer));
	if (!packer) return NULL;

	memcpy(&packer->handler, handler, sizeof(packer->handler));
	packer->cbparam = cbparam;
	packer->size = size;

	packer->pkt.rtp.v = RTP_VERSION;
	packer->pkt.rtp.pt = pt;
	packer->pkt.rtp.seq = seq;
	packer->pkt.rtp.ssrc = ssrc;
	return packer;
}

static void rtp_vp8_pack_destroy(void* pack)
{
	struct rtp_encode_vp8_t *packer;
	packer = (struct rtp_encode_vp8_t *)pack;
#if defined(_DEBUG) || defined(DEBUG)
	memset(packer, 0xCC, sizeof(*packer));
#endif
	free(packer);
}

static void rtp_vp8_pack_get_info(void* pack, uint16_t* seq, uint32_t* timestamp)
{
	struct rtp_encode_vp8_t *packer;
	packer = (struct rtp_encode_vp8_t *)pack;
	*seq = (uint16_t)packer->pkt.rtp.seq;
	*timestamp = packer->pkt.rtp.timestamp;
}

static int rtp_vp8_pack_input(void* pack, const void* data, int bytes, uint32_t timestamp)
{
	int n;
	uint8_t *rtp;
	uint8_t vp8_payload_descriptor[1];
	const uint8_t *ptr;
	struct rtp_encode_vp8_t *packer;
	packer = (struct rtp_encode_vp8_t *)pack;
	packer->pkt.rtp.timestamp = timestamp; //(uint32_t)(time * KHz);

	ptr = (const uint8_t *)data;
	for (vp8_payload_descriptor[0] = 0x10 /*start of partition*/; bytes > 0; ++packer->pkt.rtp.seq)
	{
		packer->pkt.payload = ptr;
		packer->pkt.payloadlen = (bytes + N_VP8_HEADER + RTP_FIXED_HEADER) <= packer->size ? bytes : (packer->size - N_VP8_HEADER - RTP_FIXED_HEADER);
		ptr += packer->pkt.payloadlen;
		bytes -= packer->pkt.payloadlen;

		n = RTP_FIXED_HEADER + N_VP8_HEADER + packer->pkt.payloadlen;
		rtp = (uint8_t*)packer->handler.alloc(packer->cbparam, n);
		if (!rtp) return -ENOMEM;

		// Marker bit (M): MUST be set for the very last packet of each encoded
		// frame in line with the normal use of the M bit in video formats.
		packer->pkt.rtp.m = (0 == bytes) ? 1 : 0;
		n = rtp_packet_serialize_header(&packer->pkt, rtp, n);
		if (n != RTP_FIXED_HEADER)
		{
			assert(0);
			return -1;
		}

		memcpy(rtp + n, vp8_payload_descriptor, N_VP8_HEADER);
		memcpy(rtp + n + N_VP8_HEADER, packer->pkt.payload, packer->pkt.payloadlen);
		packer->handler.packet(packer->cbparam, rtp, n + N_VP8_HEADER + packer->pkt.payloadlen, packer->pkt.rtp.timestamp, 0);
		packer->handler.free(packer->cbparam, rtp);
		vp8_payload_descriptor[0] = 0x00;
	}

	return 0;
}

struct rtp_payload_encode_t *rtp_vp8_encode()
{
	static struct rtp_payload_encode_t encode = {
		rtp_vp8_pack_create,
		rtp_vp8_pack_destroy,
		rtp_vp8_pack_get_info,
		rtp_vp8_pack_input,
	};

	return &encode;
}
