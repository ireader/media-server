#include "ctypedef.h"
#include "rtp-packet.h"
#include "rtp-payload-internal.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

// RFC6184 RTP Payload Format for H.264 Video

#define KHz         90 // 90000Hz
#define FU_START    0x80
#define FU_END      0x40

struct rtp_encode_h264_t
{
	struct rtp_packet_t pkt;
	struct rtp_payload_t handler;
	void* cbparam;
	int size;
};

static void* rtp_h264_pack_create(int size, uint8_t pt, uint16_t seq, uint32_t ssrc, uint32_t frequency, struct rtp_payload_t *handler, void* cbparam)
{
	struct rtp_encode_h264_t *packer;
	packer = (struct rtp_encode_h264_t *)calloc(1, sizeof(*packer));
	if(!packer) return NULL;

	memcpy(&packer->handler, handler, sizeof(packer->handler));
	packer->cbparam = cbparam;
	packer->size = size;

	assert(KHz * 1000 == frequency);
	packer->pkt.rtp.v = RTP_VERSION;
	packer->pkt.rtp.pt = pt;
	packer->pkt.rtp.seq = seq;
	packer->pkt.rtp.ssrc = ssrc;
	return packer;
}

static void rtp_h264_pack_destroy(void* pack)
{
	struct rtp_encode_h264_t *packer;
	packer = (struct rtp_encode_h264_t *)pack;
#if defined(_DEBUG) || defined(DEBUG)
	memset(packer, 0xCC, sizeof(*packer));
#endif
	free(packer);
}

static void rtp_h264_pack_get_info(void* pack, uint16_t* seq, uint32_t* timestamp)
{
	struct rtp_encode_h264_t *packer;
	packer = (struct rtp_encode_h264_t *)pack;
	*seq = (uint16_t)packer->pkt.rtp.seq;
	*timestamp = packer->pkt.rtp.timestamp;
}

static const uint8_t* h264_nalu_find(const uint8_t* p, size_t bytes)
{
	size_t i;
	for(i = 2; i + 1 < bytes; i++)
	{
		if (0x01 == p[i] && 0x00 == p[i - 1] && 0x00 == p[i - 2])
			return p + i + 1;
	}
	return p + bytes;
}

static int rtp_h264_pack_nalu(struct rtp_encode_h264_t *packer, const uint8_t* nalu, int bytes, int64_t time)
{
	int n;
	uint8_t *rtp;

	packer->pkt.payload = nalu;
	packer->pkt.payloadlen = bytes;
	n = RTP_FIXED_HEADER + packer->pkt.payloadlen;
	rtp = (uint8_t*)packer->handler.alloc(packer->cbparam, n);
	if (!rtp) return ENOMEM;

	packer->pkt.rtp.m = 1; // set marker flag
	n = rtp_packet_serialize(&packer->pkt, rtp, n);
	if ((size_t)n != RTP_FIXED_HEADER + packer->pkt.payloadlen)
	{
		assert(0);
		return -1;
	}

	++packer->pkt.rtp.seq;
	packer->handler.packet(packer->cbparam, rtp, n, time, 0);
	packer->handler.free(packer->cbparam, rtp);
	return 0;
}

static int rtp_h264_pack_fu_a(struct rtp_encode_h264_t *packer, const uint8_t* nalu, int bytes, int64_t time, int MAX_PACKET)
{
	int n;
	unsigned char *rtp;

	// RFC6184 5.3. NAL Unit Header Usage: Table 2 (p15)
	// RFC6184 5.8. Fragmentation Units (FUs) (p29)
	uint8_t fu_indicator = (*nalu & 0xE0) | 28; // FU-A
	uint8_t fu_header = *nalu & 0x1F;

	nalu += 1; // skip NAL Unit Type byte
	bytes -= 1;
	assert(bytes > 0);

	// FU-A start
	for (fu_header |= FU_START; bytes > 0; ++packer->pkt.rtp.seq)
	{
		if (bytes <= MAX_PACKET - 2)
		{
			assert(0 == (fu_header & FU_START));
			fu_header = FU_END | (fu_header & 0x1F); // FU-A end
			packer->pkt.payloadlen = bytes + 2/*fu_indicator + fu_header*/;
		}
		else
		{
			packer->pkt.payloadlen = MAX_PACKET;
		}

		packer->pkt.payload = nalu - 2/*fu_indicator + fu_header*/;
		n = RTP_FIXED_HEADER + packer->pkt.payloadlen;
		rtp = (uint8_t*)packer->handler.alloc(packer->cbparam, n);
		if (!rtp) return ENOMEM;

		packer->pkt.rtp.m = (FU_END & fu_header) ? 1 : 0; // set marker flag
		n = rtp_packet_serialize(&packer->pkt, rtp, n);
		if ((size_t)n != RTP_FIXED_HEADER + packer->pkt.payloadlen)
		{
			assert(0);
			return -1;
		}

		rtp[RTP_FIXED_HEADER + 0] = fu_indicator;
		rtp[RTP_FIXED_HEADER + 1] = fu_header;
		packer->handler.packet(packer->cbparam, rtp, n, time, 0);
		packer->handler.free(packer->cbparam, rtp);

		bytes -= packer->pkt.payloadlen - 2;
		nalu += packer->pkt.payloadlen - 2;
		fu_header &= 0x1F; // clear flags
	}

	return 0;
}

static int rtp_h264_pack_input(void* pack, const void* h264, int bytes, int64_t time)
{
	int r = 0;
	const uint8_t *p1, *p2, *pend;
	struct rtp_encode_h264_t *packer;
	packer = (struct rtp_encode_h264_t *)pack;
	packer->pkt.rtp.timestamp = (uint32_t)time * KHz; // ms -> 90KHZ

	pend = (const uint8_t*)h264 + bytes;
	for(p1 = h264_nalu_find((const uint8_t*)h264, bytes); p1 < pend && 0 == r; p1 = p2)
	{
		size_t nalu_size;

		// filter H.264 start code(0x00000001)
		assert(0 < (*p1 & 0x1F) && (*p1 & 0x1F) < 24);
		p2 = h264_nalu_find(p1 + 1, pend - p1 - 1);
		nalu_size = p2 - p1;
		
		// filter suffix '00' bytes
		while(0 == p1[nalu_size-1]) --nalu_size;

		if(nalu_size < (size_t)packer->size)
		{
			// single NAl unit packet 
			r = rtp_h264_pack_nalu(packer, p1, nalu_size, time);
		}
		else
		{
			r = rtp_h264_pack_fu_a(packer, p1, nalu_size, time, packer->size);
		}
	}

	return 0;
}

struct rtp_payload_encode_t *rtp_h264_encode()
{
	static struct rtp_payload_encode_t packer = {
		rtp_h264_pack_create,
		rtp_h264_pack_destroy,
		rtp_h264_pack_get_info,
		rtp_h264_pack_input,
	};

	return &packer;
}
