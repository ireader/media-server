// RTP Payload Format for VP9 Video draft-ietf-payload-vp9-03

#include "rtp-packet.h"
#include "rtp-profile.h"
#include "rtp-payload-internal.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

// Timestamp: The RTP timestamp indicates the time when the input frame was sampled, at a clock rate of 90 kHz
#define KHz 90 // 90000Hz

#define N_VP9_HEADER 1

struct rtp_encode_vp9_t
{
	struct rtp_packet_t pkt;
	struct rtp_payload_t handler;
	void* cbparam;
	int size;
};

static void* rtp_vp9_pack_create(int size, uint8_t pt, uint16_t seq, uint32_t ssrc, struct rtp_payload_t *handler, void* cbparam)
{
	struct rtp_encode_vp9_t *packer;
	packer = (struct rtp_encode_vp9_t *)calloc(1, sizeof(*packer));
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

static void rtp_vp9_pack_destroy(void* pack)
{
	struct rtp_encode_vp9_t *packer;
	packer = (struct rtp_encode_vp9_t *)pack;
#if defined(_DEBUG) || defined(DEBUG)
	memset(packer, 0xCC, sizeof(*packer));
#endif
	free(packer);
}

static void rtp_vp9_pack_get_info(void* pack, uint16_t* seq, uint32_t* timestamp)
{
	struct rtp_encode_vp9_t *packer;
	packer = (struct rtp_encode_vp9_t *)pack;
	*seq = (uint16_t)packer->pkt.rtp.seq;
	*timestamp = packer->pkt.rtp.timestamp;
}

static int rtp_vp9_pack_input(void* pack, const void* data, int bytes, uint32_t timestamp)
{
	int n;
	uint8_t *rtp;
	uint8_t vp9_payload_descriptor[1];
	const uint8_t *ptr;
	struct rtp_encode_vp9_t *packer;
	packer = (struct rtp_encode_vp9_t *)pack;
	packer->pkt.rtp.timestamp = timestamp;

	ptr = (const uint8_t *)data;
	//In non-flexible mode (with the F bit below set to 0),
	for (vp9_payload_descriptor[0] = 0x08 /*Start of a layer frame*/; bytes > 0; ++packer->pkt.rtp.seq)
	{
		packer->pkt.payload = ptr;
		packer->pkt.payloadlen = (bytes + N_VP9_HEADER + RTP_FIXED_HEADER) < packer->size ? bytes : (packer->size - N_VP9_HEADER - RTP_FIXED_HEADER);
		ptr += packer->pkt.payloadlen;
		bytes -= packer->pkt.payloadlen;

		n = RTP_FIXED_HEADER + N_VP9_HEADER + packer->pkt.payloadlen;
		rtp = (uint8_t*)packer->handler.alloc(packer->cbparam, n);
		if (!rtp) return -ENOMEM;

		// Marker bit (M): MUST be set to 1 for the final packet of the highest
		// spatial layer frame (the final packet of the super frame), and 0
		// otherwise. Unless spatial scalability is in use for this super
		// frame, this will have the same value as the E bit described below.
		// Note this bit MUST be set to 1 for the target spatial layer frame
		// if a stream is being rewritten to remove higher spatial layers.
		packer->pkt.rtp.m = (0 == bytes) ? 1 : 0;
		vp9_payload_descriptor[0] |= (0 == bytes) ? 0x04 : 0; // End of a layer frame.
		n = rtp_packet_serialize_header(&packer->pkt, rtp, n);
		if (n != RTP_FIXED_HEADER)
		{
			assert(0);
			return -1;
		}

		memcpy(rtp + n, vp9_payload_descriptor, N_VP9_HEADER);
		memcpy(rtp + n + N_VP9_HEADER, packer->pkt.payload, packer->pkt.payloadlen);
		packer->handler.packet(packer->cbparam, rtp, n + N_VP9_HEADER + packer->pkt.payloadlen, packer->pkt.rtp.timestamp, 0);
		packer->handler.free(packer->cbparam, rtp);
		vp9_payload_descriptor[0] &= ~0x08;
	}

	return 0;
}

struct rtp_payload_encode_t *rtp_vp9_encode()
{
	static struct rtp_payload_encode_t encode = {
		rtp_vp9_pack_create,
		rtp_vp9_pack_destroy,
		rtp_vp9_pack_get_info,
		rtp_vp9_pack_input,
	};

	return &encode;
}
