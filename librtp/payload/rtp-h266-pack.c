// https://www.ietf.org/archive/id/draft-ietf-avtcore-rtp-vvc-18.html
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
#define FU_MARK     0x20

#define H266_RTP_AP 28
#define H266_RTP_FU 29

#define H266_TYPE(v) (((v) >> 3) & 0x1f)
#define H266_NAL_OPI 12

#define N_FU_HEADER	3

int rtp_h264_annexb_nalu(const void* h264, int bytes, int (*handler)(void* param, const uint8_t* nalu, int bytes, int last), void* param);

struct rtp_encode_h266_t
{
	struct rtp_packet_t pkt;
	struct rtp_payload_t handler;
	void* cbparam;
	int size;
};

static void* rtp_h266_pack_create(int size, uint8_t pt, uint16_t seq, uint32_t ssrc, struct rtp_payload_t* handler, void* param)
{
	struct rtp_encode_h266_t* packer;
	packer = (struct rtp_encode_h266_t*)calloc(1, sizeof(*packer));
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

static void rtp_h266_pack_destroy(void* pack)
{
	struct rtp_encode_h266_t* packer;
	packer = (struct rtp_encode_h266_t*)pack;
#if defined(_DEBUG) || defined(DEBUG)
	memset(packer, 0xCC, sizeof(*packer));
#endif
	free(packer);
}

static void rtp_h266_pack_get_info(void* pack, uint16_t* seq, uint32_t* timestamp)
{
	struct rtp_encode_h266_t* packer;
	packer = (struct rtp_encode_h266_t*)pack;
	*seq = (uint16_t)packer->pkt.rtp.seq;
	*timestamp = packer->pkt.rtp.timestamp;
}

static int rtp_h266_pack_nalu(struct rtp_encode_h266_t* packer, const uint8_t* nalu, int bytes, int mark)
{
	int r, n;
	uint8_t* rtp;

	packer->pkt.payload = nalu;
	packer->pkt.payloadlen = bytes;
	n = RTP_FIXED_HEADER + packer->pkt.payloadlen;
	rtp = (uint8_t*)packer->handler.alloc(packer->cbparam, n);
	if (!rtp) return -ENOMEM;

	//packer->pkt.rtp.m = 1; // set marker flag
	packer->pkt.rtp.m = H266_TYPE(nalu[1]) < H266_NAL_OPI ? mark : 0; // VCL only
	n = rtp_packet_serialize(&packer->pkt, rtp, n);
	if (n != RTP_FIXED_HEADER + packer->pkt.payloadlen)
	{
		assert(0);
		return -1;
	}

	++packer->pkt.rtp.seq;
	r = packer->handler.packet(packer->cbparam, rtp, n, packer->pkt.rtp.timestamp, 0);
	packer->handler.free(packer->cbparam, rtp);
	return r;
}

static int rtp_h266_pack_fu(struct rtp_encode_h266_t* packer, const uint8_t* ptr, int bytes, int mark)
{
	int r, n;
	unsigned char* rtp;
	uint8_t fu_header;
	uint16_t nalu_header;

	if (bytes < 3)
		return -1;

	nalu_header = ((uint16_t)ptr[0] << 8) | ((ptr[1] & 0x07) | (H266_RTP_FU << 3)); // replace nalu type with 29(FU)
	fu_header = H266_TYPE(ptr[1]);

	r = 0;
	ptr += 2; // skip NAL Unit Type byte
	bytes -= 2;
	assert(bytes > 0);

	// FU-A start
	for (fu_header |= FU_START; 0 == r && bytes > 0; ++packer->pkt.rtp.seq)
	{
		if (bytes + RTP_FIXED_HEADER <= packer->size - N_FU_HEADER)
		{
			assert(0 == (fu_header & FU_START));
			fu_header = FU_END | (mark ? FU_MARK : 0) | (fu_header & 0x1F); // FU end
			packer->pkt.payloadlen = bytes;
		}
		else
		{
			packer->pkt.payloadlen = packer->size - RTP_FIXED_HEADER - N_FU_HEADER;
		}

		packer->pkt.payload = ptr;
		n = RTP_FIXED_HEADER + N_FU_HEADER + packer->pkt.payloadlen;
		rtp = (uint8_t*)packer->handler.alloc(packer->cbparam, n);
		if (!rtp) return -ENOMEM;

		packer->pkt.rtp.m = (FU_END & fu_header) ? mark : 0; // set marker flag
		n = rtp_packet_serialize_header(&packer->pkt, rtp, n);
		if (n != RTP_FIXED_HEADER)
		{
			assert(0);
			return -1;
		}

		/*header + fu_header*/
		rtp[n + 0] = (uint8_t)(nalu_header >> 8);
		rtp[n + 1] = (uint8_t)(nalu_header & 0xFF);
		rtp[n + 2] = fu_header;
		memcpy(rtp + n + N_FU_HEADER, packer->pkt.payload, packer->pkt.payloadlen);

		r = packer->handler.packet(packer->cbparam, rtp, n + N_FU_HEADER + packer->pkt.payloadlen, packer->pkt.rtp.timestamp, 0);
		packer->handler.free(packer->cbparam, rtp);

		bytes -= packer->pkt.payloadlen;
		ptr += packer->pkt.payloadlen;
		fu_header &= 0x1F; // clear flags
	}

	return r;
}

static int rtp_h266_pack_handler(void* pack, const uint8_t* nalu, int bytes, int last)
{
	struct rtp_encode_h266_t* packer;
	packer = (struct rtp_encode_h266_t*)pack;
	if (bytes + RTP_FIXED_HEADER <= packer->size)
	{
		// single NAl unit packet 
		return rtp_h266_pack_nalu(packer, nalu, bytes, last ? 1 : 0);
	}
	else
	{
		return rtp_h266_pack_fu(packer, nalu, bytes, last ? 1 : 0);
	}
}

static int rtp_h266_pack_input(void* pack, const void* h266, int bytes, uint32_t timestamp)
{
	struct rtp_encode_h266_t* packer;
	packer = (struct rtp_encode_h266_t*)pack;
	//	assert(packer->pkt.rtp.timestamp != timestamp || !packer->pkt.payload /*first packet*/);
	packer->pkt.rtp.timestamp = timestamp; //(uint32_t)time * KHz; // ms -> 90KHZ
	return rtp_h264_annexb_nalu(h266, bytes, rtp_h266_pack_handler, packer);
}

struct rtp_payload_encode_t* rtp_h266_encode()
{
	static struct rtp_payload_encode_t packer = {
		rtp_h266_pack_create,
		rtp_h266_pack_destroy,
		rtp_h266_pack_get_info,
		rtp_h266_pack_input,
	};

	return &packer;
}
