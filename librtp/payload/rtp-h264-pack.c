#include <stdlib.h>
#include <memory.h>
#include <assert.h>
#include <errno.h>
#include "ctypedef.h"
#include "rtp-pack.h"

#define RTP_HEADER_SIZE 12 // don't include RTP CSRC and RTP Header Extension
//static size_t s_max_packet_size = 576 - RTP_HEADER_SIZE; // UNIX Network Programming by W. Richard Stevens
static size_t s_max_packet_size = 1434 - RTP_HEADER_SIZE; // from VLC

struct rtp_h264_packer_t
{
	struct rtp_pack_func_t func;
	void* cbparam;
	uint32_t ssrc;
	uint32_t timestamp;
	uint16_t seq;
	uint8_t payload;
};

void rtp_pack_setsize(size_t bytes)
{
	s_max_packet_size = bytes;
}

size_t rtp_pack_getsize()
{
	return s_max_packet_size;
}

static void* rtp_h264_pack_create(uint32_t ssrc, unsigned short seq, uint8_t payload, struct rtp_pack_func_t *func, void* param)
{
	struct rtp_h264_packer_t *packer;
	packer = (struct rtp_h264_packer_t *)malloc(sizeof(*packer));
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

static void rtp_h264_pack_destroy(void* pack)
{
	struct rtp_h264_packer_t *packer;
	packer = (struct rtp_h264_packer_t *)pack;
#if defined(_DEBUG) || defined(DEBUG)
	memset(packer, 0xCC, sizeof(*packer));
#endif
	free(packer);
}

static const unsigned char* search_start_code(const unsigned char* ptr, size_t bytes)
{
	const unsigned char *p;
	for(p = ptr; p + 3 < ptr + bytes; p++)
	{
		if(0x00 == p[0] && 0x00 == p[1] && (0x01 == p[2] || (0x00==p[2] && 0x01==p[3])))
			return p;
	}
	return NULL;
}

static unsigned char* alloc_packet(struct rtp_h264_packer_t *packer, uint32_t timestamp, size_t bytes)
{
	unsigned char *rtp;
	rtp = (unsigned char*)packer->func.alloc(packer->cbparam, bytes+14);
	if(!rtp) return NULL;

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

static int rtp_h264_pack_input(void* pack, const void* h264, size_t bytes, uint64_t time)
{
	size_t MAX_PACKET;
	unsigned char *rtp;
	const unsigned char *p1, *p2;
	struct rtp_h264_packer_t *packer;
	packer = (struct rtp_h264_packer_t *)pack;

	packer->timestamp = (uint32_t)time * 90; // ms -> 90KHZ

	MAX_PACKET = rtp_pack_getsize(); // get packet size

	p1 = h264;
	assert(p1 == search_start_code(h264, bytes));

	while(bytes > 0)
	{
		size_t nalu_size;

		p2 = search_start_code(p1+3, bytes - 3);
		if(!p2) p2 = p1 + bytes;
		nalu_size = p2 - p1;
		bytes -= nalu_size;

		// filter suffix '00' bytes
		while(0 == p1[nalu_size-1]) --nalu_size;

		// filter H.264 start code(0x00000001)
		nalu_size -= (0x01 == p1[2]) ? 3 : 4;
		p1 += (0x01 == p1[2]) ? 3 : 4;
		assert(0 < (*p1 & 0x1F) && (*p1 & 0x1F) < 24);

		if(nalu_size < MAX_PACKET)
		{
			rtp = alloc_packet(packer, packer->timestamp, MAX_PACKET);
			if(!rtp) return ENOMEM;
			rtp[1] |= 0x80; // marker
			rtp[2] = (unsigned char)(packer->seq >> 8);
			rtp[3] = (unsigned char)(packer->seq);
			++packer->seq;

			memcpy(rtp+12, p1, nalu_size);

			// single NAl unit packet 
			packer->func.packet(packer->cbparam, rtp, nalu_size+12, time);
			packer->func.free(packer->cbparam, rtp);
		}
		else
		{
			// RFC6184 5.3. NAL Unit Header Usage: Table 2 (p15)
			// RFC6184 5.8. Fragmentation Units (FUs) (p29)
			unsigned char fu_indicator = (*p1 & 0xE0) | 28; // FU-A
			unsigned char fu_header = *p1 & 0x1F;

			p1 += 1; // skip NAL Unit Type byte
			nalu_size -= 1;

			// FU-A start
			fu_header = 0x80 | fu_header;
			while(nalu_size > MAX_PACKET-1)
			{
				rtp = alloc_packet(packer, packer->timestamp, MAX_PACKET);
				if(!rtp) return ENOMEM;
				rtp[1] &= ~0x80; // clean marker
				rtp[2] = (unsigned char)(packer->seq >> 8);
				rtp[3] = (unsigned char)(packer->seq);
				rtp[12] = fu_indicator;
				rtp[13] = fu_header;
				++packer->seq;

				memcpy(rtp+14, p1, MAX_PACKET-1);
				//packer->callback(packer->cbparam, fu_indicator, fu_header, p1, s_max_packet_size);
				packer->func.packet(packer->cbparam, rtp, MAX_PACKET-1+14, time);
				packer->func.free(packer->cbparam, rtp);

				nalu_size -= MAX_PACKET-1;
				p1 += MAX_PACKET-1;
				fu_header = 0x1F & fu_header; // FU-A fragment
			}

			// FU-A end
			fu_header = 0x40 | (fu_header & 0x1F);
			rtp = alloc_packet(packer, packer->timestamp, MAX_PACKET);
			if(!rtp) return ENOMEM;
			rtp[1] |= 0x80; // marker
			rtp[2] = (unsigned char)(packer->seq >> 8);
			rtp[3] = (unsigned char)(packer->seq);
			rtp[12] = fu_indicator;
			rtp[13] = fu_header;
			++packer->seq;

			while(nalu_size > 1 && 0 == p1[nalu_size-1])
				--nalu_size;

			memcpy(rtp+14, p1, nalu_size);
			packer->func.packet(packer->cbparam, rtp, nalu_size+14, time);
			packer->func.free(packer->cbparam, rtp);
		}

		p1 = p2;
	}

	return 0;
}

static void rtp_h264_pack_get_info(void* pack, unsigned short* seq, unsigned int* timestamp)
{
	struct rtp_h264_packer_t *packer;
	packer = (struct rtp_h264_packer_t *)pack;
	*seq = packer->seq;
	*timestamp = packer->timestamp;
}

struct rtp_pack_t *rtp_h264_packer()
{
	static struct rtp_pack_t packer = {
		rtp_h264_pack_create,
		rtp_h264_pack_destroy,
		rtp_h264_pack_get_info,
		rtp_h264_pack_input,
	};

	return &packer;
}
