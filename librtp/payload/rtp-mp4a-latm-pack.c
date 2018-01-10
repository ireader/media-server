/// RFC3016 RTP Payload Format for MPEG-4 Audio/Visual Streams
/// RFC6416 RTP Payload Format for MPEG-4 Audio/Visual Streams
///
/// MPEG-4 Audio streams MUST be formatted LATM (Lowoverhead
/// MPEG-4 Audio Transport Multiplex)[14496 - 3] streams, and the
/// LATM-based streams are then mapped onto RTP packets

#include "rtp-packet.h"
#include "rtp-profile.h"
#include "rtp-payload-internal.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

struct rtp_encode_mp4a_latm_t
{
	struct rtp_packet_t pkt;
	struct rtp_payload_t handler;
	void* cbparam;
	int size;
};

static void* rtp_mp4a_latm_pack_create(int size, uint8_t pt, uint16_t seq, uint32_t ssrc, struct rtp_payload_t *handler, void* cbparam)
{
	struct rtp_encode_mp4a_latm_t *packer;
	packer = (struct rtp_encode_mp4a_latm_t *)calloc(1, sizeof(*packer));
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

static void rtp_mp4a_latm_pack_destroy(void* pack)
{
	struct rtp_encode_mp4a_latm_t *packer;
	packer = (struct rtp_encode_mp4a_latm_t *)pack;
#if defined(_DEBUG) || defined(DEBUG)
	memset(packer, 0xCC, sizeof(*packer));
#endif
	free(packer);
}

static void rtp_mp4a_latm_pack_get_info(void* pack, uint16_t* seq, uint32_t* timestamp)
{
	struct rtp_encode_mp4a_latm_t *packer;
	packer = (struct rtp_encode_mp4a_latm_t *)pack;
	*seq = (uint16_t)packer->pkt.rtp.seq;
	*timestamp = packer->pkt.rtp.timestamp;
}

static int rtp_mp4a_latm_pack_input(void* pack, const void* data, int bytes, uint32_t timestamp)
{
	int n, len;
	uint8_t *rtp;
	uint8_t hd[400]; // 100KB
	const uint8_t *ptr;
	struct rtp_encode_mp4a_latm_t *packer;
	packer = (struct rtp_encode_mp4a_latm_t *)pack;
	assert(packer->pkt.rtp.timestamp != timestamp || !packer->pkt.payload /*first packet*/);
	packer->pkt.rtp.timestamp = timestamp; //(uint32_t)(time * KHz); // ms -> 90KHZ (RFC2250 section2 p2)

	ptr = (const uint8_t *)data;
	if (0xFF == ptr[0] && 0xF0 == (ptr[1] & 0xF0) && bytes > 7)
	{
		// skip ADTS header
		assert(bytes == (((ptr[3] & 0x03) << 11) | (ptr[4] << 3) | ((ptr[5] >> 5) & 0x07)));
		ptr += 7;
		bytes -= 7;
	}

	// ISO/IEC 14496-3:200X(E)
	// Table 1.44 ¨C Syntax of PayloadLengthInfo() (p84)
	len = bytes / 255 + 1;
	if (len > sizeof(hd))
	{
		assert(0);
		return -E2BIG; // invalid packet
	}
	memset(hd, 255, len - 1);
	hd[len - 1] = bytes % 255;

	for (; bytes > 0; ++packer->pkt.rtp.seq)
	{
		packer->pkt.payload = ptr;
		packer->pkt.payloadlen = (bytes + len + RTP_FIXED_HEADER) <= packer->size ? bytes : (packer->size - len - RTP_FIXED_HEADER);
		ptr += packer->pkt.payloadlen;
		bytes -= packer->pkt.payloadlen;

		n = RTP_FIXED_HEADER + len + packer->pkt.payloadlen;
		rtp = (uint8_t*)packer->handler.alloc(packer->cbparam, n);
		if (!rtp) return -ENOMEM;

		// Marker (M) bit: The marker bit indicates audioMuxElement boundaries.
		// It is set to 1 to indicate that the RTP packet contains a complete
		// audioMuxElement or the last fragment of an audioMuxElement.
		packer->pkt.rtp.m = (0 == bytes) ? 1 : 0;
		n = rtp_packet_serialize_header(&packer->pkt, rtp, n);
		if (n != RTP_FIXED_HEADER)
		{
			assert(0);
			return -1;
		}

		if (len > 0) memcpy(rtp + n, hd, len);
		memcpy(rtp + n + len, packer->pkt.payload, packer->pkt.payloadlen);
		packer->handler.packet(packer->cbparam, rtp, n + len + packer->pkt.payloadlen, packer->pkt.rtp.timestamp, 0);
		packer->handler.free(packer->cbparam, rtp);
		len = 0;
	}

	return 0;
}

struct rtp_payload_encode_t *rtp_mp4a_latm_encode()
{
	static struct rtp_payload_encode_t encode = {
		rtp_mp4a_latm_pack_create,
		rtp_mp4a_latm_pack_destroy,
		rtp_mp4a_latm_pack_get_info,
		rtp_mp4a_latm_pack_input,
	};

	return &encode;
}
