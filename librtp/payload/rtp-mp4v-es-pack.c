/// RFC6416 RTP Payload Format for MPEG-4 Audio/Visual Streams
///
/// 5.1. Use of RTP Header Fields for MPEG-4 Visual (p9)
/// Marker (M) bit: The marker bit is set to 1 to indicate the last RTP
/// packet(or only RTP packet) of a VOP.When multiple VOPs are carried
/// in the same RTP packet, the marker bit is set to 1.
///
/// 5.2. Fragmentation of MPEG-4 Visual Bitstream
/// A fragmented MPEG-4 Visual bitstream is mapped directly onto the RTP
/// payload without any addition of extra header fields or any removal of
/// Visual syntax elements.
/// 
/// 6.2. Use of RTP Header Fields for MPEG-4 Audio (p16)
/// Marker (M) bit: The marker bit indicates audioMuxElement boundaries.
/// It is set to 1 to indicate that the RTP packet contains a complete
/// audioMuxElement or the last fragment of an audioMuxElement
///
/// 6.3. Fragmentation of MPEG-4 Audio Bitstream
/// It is RECOMMENDED to put one audioMuxElement in each RTP packet.

#include "rtp-packet.h"
#include "rtp-profile.h"
#include "rtp-payload-internal.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#define KHz 90 // 90000Hz

struct rtp_encode_mp4v_es_t
{
	struct rtp_packet_t pkt;
	struct rtp_payload_t handler;
	void* cbparam;
	int size;
};

static void* rtp_mp4v_es_encode_create(int size, uint8_t pt, uint16_t seq, uint32_t ssrc, struct rtp_payload_t *handler, void* cbparam)
{
	struct rtp_encode_mp4v_es_t *packer;
	packer = (struct rtp_encode_mp4v_es_t *)calloc(1, sizeof(*packer));
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

static void rtp_mp4v_es_encode_destroy(void* pack)
{
	struct rtp_encode_mp4v_es_t *packer;
	packer = (struct rtp_encode_mp4v_es_t *)pack;
#if defined(_DEBUG) || defined(DEBUG)
	memset(packer, 0xCC, sizeof(*packer));
#endif
	free(packer);
}

static void rtp_mp4v_es_encode_get_info(void* pack, uint16_t* seq, uint32_t* timestamp)
{
	struct rtp_encode_mp4v_es_t *packer;
	packer = (struct rtp_encode_mp4v_es_t *)pack;
	*seq = (uint16_t)packer->pkt.rtp.seq;
	*timestamp = packer->pkt.rtp.timestamp;
}

static int rtp_mp4v_es_encode_input(void* pack, const void* data, int bytes, uint32_t timestamp)
{
	int n;
	uint8_t *rtp;
	const uint8_t *ptr;
	struct rtp_encode_mp4v_es_t *packer;
	packer = (struct rtp_encode_mp4v_es_t *)pack;
	assert(packer->pkt.rtp.timestamp != timestamp || !packer->pkt.payload /*first packet*/);
	packer->pkt.rtp.timestamp = timestamp; //(uint32_t)(time * KHz);

	for (ptr = (const uint8_t *)data; bytes > 0; ++packer->pkt.rtp.seq)
	{
		packer->pkt.payload = ptr;
		packer->pkt.payloadlen = (bytes + RTP_FIXED_HEADER) <= packer->size ? bytes : (packer->size - RTP_FIXED_HEADER);
		ptr += packer->pkt.payloadlen;
		bytes -= packer->pkt.payloadlen;

		n = RTP_FIXED_HEADER + packer->pkt.payloadlen;
		rtp = (uint8_t*)packer->handler.alloc(packer->cbparam, n);
		if (!rtp) return ENOMEM;

		packer->pkt.rtp.m = (0 == bytes) ? 1 : 0;
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

struct rtp_payload_encode_t *rtp_mp4v_es_encode()
{
	static struct rtp_payload_encode_t encode = {
		rtp_mp4v_es_encode_create,
		rtp_mp4v_es_encode_destroy,
		rtp_mp4v_es_encode_get_info,
		rtp_mp4v_es_encode_input,
	};

	return &encode;
}
