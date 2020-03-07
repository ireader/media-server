// RFC7798 RTP Payload Format for High Efficiency Video Coding (HEVC)
//
// 4.1. RTP Header Usage (p20)
// The RTP timestamp is set to the sampling timestamp of the content. A 90 kHz clock rate MUST be used.

#include "rtp-packet.h"
#include "rtp-payload-internal.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#define KHz         90 // 90000Hz
#define FU_START    0x80
#define FU_END      0x40

#define N_FU_HEADER	3

struct rtp_encode_h265_t
{
	struct rtp_packet_t pkt;
	struct rtp_payload_t handler;
	void* cbparam;
	int size;
};

static void* rtp_h265_pack_create(int size, uint8_t pt, uint16_t seq, uint32_t ssrc, struct rtp_payload_t *handler, void* param)
{
	struct rtp_encode_h265_t *packer;
	packer = (struct rtp_encode_h265_t *)calloc(1, sizeof(*packer));
	if (!packer) return NULL;

	memcpy(&packer->handler, handler, sizeof(packer->handler));
	packer->cbparam = param;
	packer->size = size;

	packer->pkt.rtp.v = RTP_VERSION;
	packer->pkt.rtp.pt = pt;
	packer->pkt.rtp.seq = seq;
	packer->pkt.rtp.ssrc = ssrc;
	return packer;
}

static void rtp_h265_pack_destroy(void* pack)
{
	struct rtp_encode_h265_t *packer;
	packer = (struct rtp_encode_h265_t *)pack;
#if defined(_DEBUG) || defined(DEBUG)
	memset(packer, 0xCC, sizeof(*packer));
#endif
	free(packer);
}

static const uint8_t* h265_nalu_find(const uint8_t* p, const uint8_t* end)
{
	for (p += 2; p + 1 < end; p++)
	{
		if (0x01 == *p && 0x00 == *(p - 1) && 0x00 == *(p - 2))
			return p + 1;
	}
	return end;
}

static void rtp_h265_pack_get_info(void* pack, uint16_t* seq, uint32_t* timestamp)
{
	struct rtp_encode_h265_t *packer;
	packer = (struct rtp_encode_h265_t *)pack;
	*seq = (uint16_t)packer->pkt.rtp.seq;
	*timestamp = packer->pkt.rtp.timestamp;
}

static int rtp_h265_pack_nalu(struct rtp_encode_h265_t *packer, const uint8_t* nalu, int bytes)
{
	int n;
	uint8_t *rtp;

	packer->pkt.payload = nalu;
	packer->pkt.payloadlen = bytes;
	n = RTP_FIXED_HEADER + packer->pkt.payloadlen;
	rtp = (uint8_t*)packer->handler.alloc(packer->cbparam, n);
	if (!rtp) return ENOMEM;

	//packer->pkt.rtp.m = 1; // set marker flag
	packer->pkt.rtp.m = ((*nalu >> 1) & 0x3f) < 32 ? 1 : 0; // VCL only
	n = rtp_packet_serialize(&packer->pkt, rtp, n);
	if (n != RTP_FIXED_HEADER + packer->pkt.payloadlen)
	{
		assert(0);
		return -1;
	}

	++packer->pkt.rtp.seq;
	packer->handler.packet(packer->cbparam, rtp, n, packer->pkt.rtp.timestamp, 0);
	packer->handler.free(packer->cbparam, rtp);
	return 0;
}

static int rtp_h265_pack_fu(struct rtp_encode_h265_t *packer, const uint8_t* ptr, int bytes)
{
	int n;
	unsigned char *rtp;

	uint8_t fu_header = (ptr[0] >> 1) & 0x3F;

	ptr += 2; // skip NAL Unit Type byte
	bytes -= 2;
	assert(bytes > 0);

	// FU-A start
	for (fu_header |= FU_START; bytes > 0; ++packer->pkt.rtp.seq)
	{
		if (bytes + RTP_FIXED_HEADER <= packer->size - N_FU_HEADER)
		{
			assert(0 == (fu_header & FU_START));
			fu_header = FU_END | (fu_header & 0x3F); // FU end
			packer->pkt.payloadlen = bytes;
		}
		else
		{
			packer->pkt.payloadlen = packer->size - RTP_FIXED_HEADER - N_FU_HEADER;
		}

		packer->pkt.payload = ptr;
		n = RTP_FIXED_HEADER + N_FU_HEADER + packer->pkt.payloadlen;
		rtp = (uint8_t*)packer->handler.alloc(packer->cbparam, n);
		if (!rtp) return ENOMEM;

		packer->pkt.rtp.m = (FU_END & fu_header) ? 1 : 0; // set marker flag
		n = rtp_packet_serialize_header(&packer->pkt, rtp, n);
		if (n != RTP_FIXED_HEADER)
		{
			assert(0);
			return -1;
		}

		/*header + fu_header*/
		rtp[n + 0] = 49 << 1;
		rtp[n + 1] = 1;
		rtp[n + 2] = fu_header;
		memcpy(rtp + n + N_FU_HEADER, packer->pkt.payload, packer->pkt.payloadlen);

		packer->handler.packet(packer->cbparam, rtp, n + N_FU_HEADER + packer->pkt.payloadlen, packer->pkt.rtp.timestamp, 0);
		packer->handler.free(packer->cbparam, rtp);

		bytes -= packer->pkt.payloadlen;
		ptr += packer->pkt.payloadlen;
		fu_header &= 0x3F; // clear flags
	}

	return 0;
}

static int rtp_h265_pack_input(void* pack, const void* h265, int bytes, uint32_t timestamp)
{
	int r = 0;
	const uint8_t *p1, *p2, *pend;
	struct rtp_encode_h265_t *packer;
	packer = (struct rtp_encode_h265_t *)pack;
//	assert(packer->pkt.rtp.timestamp != timestamp || !packer->pkt.payload /*first packet*/);
	packer->pkt.rtp.timestamp = timestamp; //(uint32_t)time * KHz; // ms -> 90KHZ

	pend = (const uint8_t*)h265 + bytes;
	for (p1 = h265_nalu_find((const uint8_t*)h265, pend); p1 < pend && 0 == r; p1 = p2)
	{
		size_t nalu_size;

		// filter H.265 start code(0x00000001)
		p2 = h265_nalu_find(p1 + 1, pend);
		nalu_size = p2 - p1;

		// filter suffix '00' bytes
		if (p2 != pend) --nalu_size;
		while (0 == p1[nalu_size - 1]) --nalu_size;

		if (nalu_size + RTP_FIXED_HEADER <= (size_t)packer->size)
		{
			// single NAl unit packet 
			r = rtp_h265_pack_nalu(packer, p1, (int)nalu_size);
		}
		else
		{
			r = rtp_h265_pack_fu(packer, p1, (int)nalu_size);
		}
	}

	return 0;
}

struct rtp_payload_encode_t *rtp_h265_encode()
{
	static struct rtp_payload_encode_t packer = {
		rtp_h265_pack_create,
		rtp_h265_pack_destroy,
		rtp_h265_pack_get_info,
		rtp_h265_pack_input,
	};

	return &packer;
}
