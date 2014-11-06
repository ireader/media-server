#include <stdlib.h>
#include "cstringext.h"
#include "ctypedef.h"
#include "rtp-h264-pack.h"

#define RTP_HEADER_SIZE 12 // don't include RTP CSRC and RTP Header Extension
static size_t s_max_packet_size = 576 - RTP_HEADER_SIZE; // UNIX Network Programming by W. Richard Stevens

struct rtp_h264_packer_t
{
	rtp_h264_pack_onrtp callback;
	void* cbparam;
};

void* rtp_h264_pack_create(rtp_h264_pack_onrtp callback, void* param)
{
	struct rtp_h264_packer_t *packer;
	packer = (struct rtp_h264_packer_t *)malloc(sizeof(*packer));
	if(!packer) return NULL;

	memset(packer, 0, sizeof(*packer));
	packer->callback = callback;
	packer->cbparam = param;

	return packer;
}

void rtp_h264_pack_destroy(void* pack)
{
	struct rtp_h264_packer_t *packer;
	packer = (struct rtp_h264_packer_t *)pack;

	free(packer);
}

inline const unsigned char* search_start_code(const unsigned char* ptr, size_t bytes)
{
	const unsigned char *p;
	for(p = ptr; p + 3 < ptr + bytes; p++)
	{
		if(0x00 == p[0] && 0x00 == p[1] && (0x01 == p[2] || (0x00==p[2] && 0x01==p[3])))
			return p;
	}
	return NULL;
}

int rtp_h264_pack_input(void* pack, const void* h264, size_t bytes)
{
	const unsigned char *p1, *p2;
	struct rtp_h264_packer_t *packer;
	packer = (struct rtp_h264_packer_t *)pack;

	p1 = (const unsigned char *)h264;
	assert(p1 == search_start_code(p1, bytes));

	while(bytes > 0)
	{
		size_t nalu_size;

		p2 = search_start_code(p1+3, bytes - 3);
		if(!p2) p2 = p1 + bytes;
		nalu_size = p2 - p1;
		bytes -= nalu_size;

		// filter suffix '00' bytes
		while(0 == p2[nalu_size-1]) --nalu_size;

		// filter H.264 start code(0x00000001)
		nalu_size -= (0x01 == p1[2]) ? 3 : 4;
		p1 += (0x01 == p1[2]) ? 3 : 4;
		assert(0 < (*p1 & 0x1F) && (*p1 & 0x1F) < 24);

		if(nalu_size < s_max_packet_size)
		{
			// single NAl unit packet 
			//packer->callback(packer->cbparam, p1, nalu_size);
		}
		else
		{
			// RFC6184 5.3. NAL Unit Header Usage: Table 2 (p15)
			// RFC6184 5.8. Fragmentation Units (FUs) (p29)
			unsigned char fu_indicator = (*p1 & 0xE0) | 28; // FU-A
			unsigned char fu_header = *p1 & 0x1F;

			// FU-A start
			fu_header = 0x9F & fu_header;
			while(nalu_size > s_max_packet_size)
			{
				//packer->callback(packer->cbparam, fu_indicator, fu_header, p1, s_max_packet_size);

				nalu_size -= s_max_packet_size;
				p1 += s_max_packet_size;
				fu_header = 0x1F & fu_header; // FU-A fragment
			}

			// FU-A end
			fu_header = (0x5F & fu_header);
			//packer->callback(packer->cbparam, fu_indicator, fu_header, p1, nalu_size);
		}

		p1 = p2;
	}

	return 0;
}

void rtp_h264_pack_set_size(size_t max_packet_bytes)
{
	s_max_packet_size = max_packet_bytes;
}

size_t rtp_h264_pack_get_size()
{
	return s_max_packet_size;
}
