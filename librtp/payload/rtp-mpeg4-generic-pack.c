// RFC3640 RTP Payload Format for Transport of MPEG-4 Elementary Streams
//
// Indicates the sampling instant of the first AU contained
// in the RTP payload. This sampling instant is equivalent to the
// CTS in the MPEG-4 time domain. When using SDP, the clock rate of
// the RTP time stamp MUST be expressed using the "rtpmap" attribute.
// If an MPEG-4 audio stream is transported, the rate SHOULD be set
// to the same value as the sampling rate of the audio stream. If an
// MPEG-4 video stream is transported, it is RECOMMENDED that the
// rate be set to 90 kHz.

#include "rtp-packet.h"
#include "rtp-profile.h"
#include "rtp-payload-internal.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#define KHz 90 // 90000Hz

#define N_AU_HEADER 4

struct rtp_encode_mpeg4_generic_t
{
	struct rtp_packet_t pkt;
	struct rtp_payload_t handler;
	void* cbparam;
	int size;
};

static void* rtp_mpeg4_generic_pack_create(int size, uint8_t pt, uint16_t seq, uint32_t ssrc, struct rtp_payload_t *handler, void* cbparam)
{
	struct rtp_encode_mpeg4_generic_t *packer;
	packer = (struct rtp_encode_mpeg4_generic_t *)calloc(1, sizeof(*packer));
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

static void rtp_mpeg4_generic_pack_destroy(void* pack)
{
	struct rtp_encode_mpeg4_generic_t *packer;
	packer = (struct rtp_encode_mpeg4_generic_t *)pack;
#if defined(_DEBUG) || defined(DEBUG)
	memset(packer, 0xCC, sizeof(*packer));
#endif
	free(packer);
}

static void rtp_mpeg4_generic_pack_get_info(void* pack, uint16_t* seq, uint32_t* timestamp)
{
	struct rtp_encode_mpeg4_generic_t *packer;
	packer = (struct rtp_encode_mpeg4_generic_t *)pack;
	*seq = (uint16_t)packer->pkt.rtp.seq;
	*timestamp = packer->pkt.rtp.timestamp;
}

static int rtp_mpeg4_generic_pack_input(void* pack, const void* data, int bytes, uint32_t timestamp)
{
	int n, size;
	uint8_t *rtp;
	uint8_t header[4];
	const uint8_t *ptr;
	struct rtp_encode_mpeg4_generic_t *packer;
	packer = (struct rtp_encode_mpeg4_generic_t *)pack;
	packer->pkt.rtp.timestamp = timestamp; //(uint32_t)(time * KHz);

	ptr = (const uint8_t *)data;
	if (0xFF == ptr[0] && 0xF0 == (ptr[1] & 0xF0) && bytes > 7)
	{
		// skip ADTS header
		assert(bytes == (((ptr[3] & 0x03) << 11) | (ptr[4] << 3) | ((ptr[5] >> 5) & 0x07)));
		ptr += 7;
		bytes -= 7;
	}

	for (size = bytes; bytes > 0; ++packer->pkt.rtp.seq)
	{
		// 3.3.6. High Bit-rate AAC
		// SDP fmtp: sizeLength=13; indexLength=3; indexDeltaLength = 3;
		header[0] = 0;
		header[1] = 16; // 16-bits AU headers-lenght
		header[2] = (uint8_t)(size >> 5);
		header[3] = (uint8_t)(size & 0x1f) << 3;

		packer->pkt.payload = ptr;
		packer->pkt.payloadlen = (bytes + N_AU_HEADER + RTP_FIXED_HEADER) <= packer->size ? bytes : (packer->size - N_AU_HEADER - RTP_FIXED_HEADER);
		ptr += packer->pkt.payloadlen;
		bytes -= packer->pkt.payloadlen;

		n = RTP_FIXED_HEADER + N_AU_HEADER + packer->pkt.payloadlen;
		rtp = (uint8_t*)packer->handler.alloc(packer->cbparam, n);
		if (!rtp) return -ENOMEM;

		// Marker (M) bit: The M bit is set to 1 to indicate that the RTP packet
		// payload contains either the final fragment of a fragmented Access
		// Unit or one or more complete Access Units
		packer->pkt.rtp.m = (0 == bytes) ? 1 : 0;
		n = rtp_packet_serialize_header(&packer->pkt, rtp, n);
		if (n != RTP_FIXED_HEADER)
		{
			assert(0);
			return -1;
		}

		memcpy(rtp + n, header, N_AU_HEADER);
		memcpy(rtp + n + N_AU_HEADER, packer->pkt.payload, packer->pkt.payloadlen);
		packer->handler.packet(packer->cbparam, rtp, n + N_AU_HEADER + packer->pkt.payloadlen, packer->pkt.rtp.timestamp, 0);
		packer->handler.free(packer->cbparam, rtp);
	}

	return 0;
}

struct rtp_payload_encode_t *rtp_mpeg4_generic_encode()
{
	static struct rtp_payload_encode_t encode = {
		rtp_mpeg4_generic_pack_create,
		rtp_mpeg4_generic_pack_destroy,
		rtp_mpeg4_generic_pack_get_info,
		rtp_mpeg4_generic_pack_input,
	};

	return &encode;
}
