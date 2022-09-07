// https://aomediacodec.github.io/av1-rtp-spec/
// 7.1. Media Type Definition: video/av1

#include "rtp-packet.h"
#include "rtp-profile.h"
#include "rtp-payload-internal.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

// Timestamp: The RTP timestamp indicates the time when the input frame was sampled, at a clock rate of 90 kHz
#define KHz 90 // 90000Hz

#define N_AV1_HEADER 1

#define OBU_SEQUENCE_HEADER		1
#define OBU_TEMPORAL_DELIMITER	2
#define OBU_FRAME_HEADER		3
#define OBU_TILE_GROUP			4
#define OBU_METADATA			5
#define OBU_FRAME				6
#define OBU_REDUNDANT_FRAME_HEADER 7
#define OBU_TILE_LIST			8

#define AV1_AGGREGATION_HEADER_Z 0x80 // set to 1 if the first OBU element is an OBU fragment that is a continuation of an OBU fragment from the previous packet, 0 otherwise.
#define AV1_AGGREGATION_HEADER_Y 0x40 // set to 1 if the last OBU element is an OBU fragment that will continue in the next packet, 0 otherwise.
#define AV1_AGGREGATION_HEADER_N 0x08 // set to 1 if the packet is the first packet of a coded video sequence, 0 otherwise. Note: if N equals 1 then Z must equal 0.

struct rtp_encode_av1_t
{
	struct rtp_packet_t pkt;
	struct rtp_payload_t handler;
	void* cbparam;
	int size;

	uint8_t* ptr;
	int offset;

	uint8_t aggregation;
};

static inline const uint8_t* leb128(const uint8_t* data, size_t bytes, uint64_t* size)
{
	size_t i;
	for (*size = i = 0; i * 7 < 64 && i < bytes;)
	{
		*size |= ((uint64_t)(data[i] & 0x7F)) << (i * 7);
		if (0 == (data[i++] & 0x80))
			break;
	}
	return data + i;
}

static inline uint8_t* leb128_write(int64_t size, uint8_t* data, size_t bytes)
{
	size_t i;
	for (i = 0; i * 7 < 64 && i < bytes;)
	{
		data[i] = (uint8_t)(size & 0x7F);
		size >>= 7;
		data[i++] |= size > 0 ? 0x80 : 0;
		if (0 == size)
			break;
	}
	return data + i;
}

static void* rtp_av1_pack_create(int size, uint8_t pt, uint16_t seq, uint32_t ssrc, struct rtp_payload_t *handler, void* cbparam)
{
	struct rtp_encode_av1_t *packer;
	packer = (struct rtp_encode_av1_t *)calloc(1, sizeof(*packer));
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

static void rtp_av1_pack_destroy(void* pack)
{
	struct rtp_encode_av1_t *packer;
	packer = (struct rtp_encode_av1_t *)pack;
#if defined(_DEBUG) || defined(DEBUG)
	memset(packer, 0xCC, sizeof(*packer));
#endif
	free(packer);
}

static void rtp_av1_pack_get_info(void* pack, uint16_t* seq, uint32_t* timestamp)
{
	struct rtp_encode_av1_t *packer;
	packer = (struct rtp_encode_av1_t *)pack;
	*seq = (uint16_t)packer->pkt.rtp.seq;
	*timestamp = packer->pkt.rtp.timestamp;
}

static int rtp_av1_pack_flush(struct rtp_encode_av1_t *packer, uint8_t aggregation)
{
	int r, n;
	if (!packer->ptr || packer->offset <= RTP_FIXED_HEADER)
		return 0; // nothing to send

	packer->ptr[RTP_FIXED_HEADER] = aggregation;
	packer->pkt.payloadlen = packer->offset - RTP_FIXED_HEADER;
	n = rtp_packet_serialize_header(&packer->pkt, packer->ptr, packer->size);
	if (n != RTP_FIXED_HEADER)
	{
		assert(0);
		return -1;
	}

	++packer->pkt.rtp.seq;
	packer->pkt.rtp.m = 0; // clear marker bit
	packer->aggregation &= ~(AV1_AGGREGATION_HEADER_N | AV1_AGGREGATION_HEADER_Z);

	r = packer->handler.packet(packer->cbparam, packer->ptr, n + packer->pkt.payloadlen, packer->pkt.rtp.timestamp, 0);
	packer->handler.free(packer->cbparam, packer->ptr);
	packer->offset = 0;
	packer->ptr = NULL;
	return r;
}

static int rtp_av1_pack_obu(struct rtp_encode_av1_t *packer, const uint8_t* obu, int64_t bytes)
{
	int r;
	int64_t n;
	uint8_t* ptr, *end;
	
	while (bytes > 0)
	{
		if (NULL == packer->ptr)
		{
			packer->ptr = (uint8_t*)packer->handler.alloc(packer->cbparam, packer->size);
			if (!packer->ptr)
				return -ENOMEM;
			packer->offset = RTP_FIXED_HEADER + 1; // RTP Header + AV1 aggregation header
		}

		ptr = packer->ptr + packer->offset;
		end = packer->ptr + packer->size;

		// OBU element size
		assert(packer->size < 0x3FFF); // 14bits
		if (ptr + bytes + ((bytes > 0x7F) ? 2 : 1) > end)
			n = end - ptr - 2;
		else
			n = bytes;

		ptr = leb128_write(n, ptr, end - ptr);
		memcpy(ptr, obu, (size_t)n);
		ptr += n;
		obu += n;
		bytes -= n;
		packer->offset = (int)(ptr - packer->ptr);

		if (packer->size - packer->offset < 8)
		{
			r = rtp_av1_pack_flush(packer, packer->aggregation | (bytes > 0 ? AV1_AGGREGATION_HEADER_Y : 0));
			if (0 != r) return r;
		}

		if (bytes > 0)
			packer->aggregation |= AV1_AGGREGATION_HEADER_Z;
	}

	return 0;
}

/// https://aomediacodec.github.io/av1-spec/av1-spec.pd
/// Annex B: Length delimited bitstream format
/// @param[in] data temporal_unit
/// @param[in] bytes temporal_unit_sizetemporal_unit_size
static int rtp_av1_pack_input_annexb(void* pack, const void* data, int bytes, uint32_t timestamp)
{
	int r;
//	uint8_t obu_has_size_field;
	uint8_t obu_extension_flag;
	uint8_t temporal_id, temporal_id0;
	uint8_t spatial_id, spatial_id0;
	uint8_t obu_type;
	uint64_t obu_size, frame_size;
	const uint8_t *ptr, *end, *frame_end, *obu_end;
	struct rtp_encode_av1_t *packer;
	packer = (struct rtp_encode_av1_t *)pack;
	packer->pkt.rtp.timestamp = timestamp;
	packer->pkt.rtp.m = 0;
	packer->ptr = NULL; // TODO: ptr memory leak

	temporal_id0 = spatial_id0 = 0;
	ptr = (const uint8_t *)data;
	end = ptr + bytes;
	for (packer->aggregation = AV1_AGGREGATION_HEADER_N; ptr < end; ptr = frame_end)
	{
		ptr = leb128(ptr, end - ptr, &frame_size);
		frame_end = ptr + frame_size;
		if (frame_end > end)
		{
			assert(0);
			return -1;
		}

		for (; ptr < frame_end; ptr = obu_end)
		{
			ptr = leb128(ptr, bytes, &obu_size);
			obu_end = ptr + obu_size;
			if (obu_end > frame_end)
			{
				assert(0);
				return -1;
			}

			obu_type = (*ptr >> 3) & 0x0F;
			obu_extension_flag = *ptr & 0x04;
			//obu_has_size_field = *ptr & 0x02;
			if (obu_extension_flag)
			{
				temporal_id = (ptr[1] >> 5) & 0x07;
				spatial_id = (ptr[1] >> 3) & 0x03;

				// If more than one OBU contained in an RTP packet has an OBU extension header 
				// then the values of the temporal_id and spatial_id must be the same in all such 
				// OBUs in the RTP packet.
				if (temporal_id != temporal_id0 || spatial_id != spatial_id0)
				{
					r = rtp_av1_pack_flush(packer, packer->aggregation);
					if (0 != r) return r;

					temporal_id0 = temporal_id;
					spatial_id0 = spatial_id;
				}
			}

			// 5. Packetization rules
			// The temporal delimiter OBU, if present, SHOULD be removed 
			// when transmitting, and MUST be ignored by receivers.
			if (OBU_TEMPORAL_DELIMITER == obu_type)
				continue;

			if (0 != rtp_av1_pack_obu(packer, ptr, obu_size))
				return -ENOMEM;
		}
	}

	// The RTP header Marker bit MUST be set equal to 0 if the packet is not the last 
	// packet of the temporal unit, it SHOULD be set equal to 1 otherwise.
	// Note: It is possible for a receiver to receive the last packet of a temporal unit 
	// without the marker bit being set equal to 1, and a receiver should be able to handle 
	// this case. The last packet of a temporal unit is also indicated by the next packet, 
	// in RTP sequence number order, having an incremented timestamp.
	packer->pkt.rtp.m = 1;
	return rtp_av1_pack_flush(packer, packer->aggregation);
}

/// http://aomedia.org/av1/specification/syntax/#general-obu-syntax
/// Low overhead bitstream format
static int rtp_av1_pack_input_obu(void* pack, const void* data, int bytes, uint32_t timestamp)
{
	int r;
	size_t i;
	size_t offset;
	uint64_t len;
	uint8_t obu_type;
	const uint8_t* ptr, *raw;
	struct rtp_encode_av1_t* packer;

	packer = (struct rtp_encode_av1_t*)pack;
	packer->pkt.rtp.timestamp = timestamp;
	packer->pkt.rtp.m = 0;
	packer->ptr = NULL; // TODO: ptr memory leak
	packer->aggregation = 0;

	raw = (const uint8_t*)data;
	for (i = r = 0; i < bytes && 0 == r; i += (size_t)len)
	{
		// http://aomedia.org/av1/specification/syntax/#obu-header-syntax
		obu_type = (raw[i] >> 3) & 0x0F;
		if (raw[i] & 0x04) // obu_extension_flag
		{
			// http://aomedia.org/av1/specification/syntax/#obu-extension-header-syntax
			// temporal_id = (obu[1] >> 5) & 0x07;
			// spatial_id = (obu[1] >> 3) & 0x03;
			offset = 2;
		}
		else
		{
			offset = 1;
		}

		if (raw[i] & 0x02) // obu_has_size_field
		{
			ptr = leb128(raw + i + offset, (int)(bytes - i - offset), &len);
			if (ptr + len > raw + bytes)
				return -1;
			len += ptr - raw - i;
		}
		else
		{
			len = bytes - i;
		}

		// 5. Packetization rules
		// The temporal delimiter OBU, if present, SHOULD be removed 
		// when transmitting, and MUST be ignored by receivers.
		if (OBU_TEMPORAL_DELIMITER == obu_type)
			continue;

		packer->aggregation |= OBU_SEQUENCE_HEADER == obu_type ? AV1_AGGREGATION_HEADER_N : 0;
		r = rtp_av1_pack_obu(packer, raw + i, (size_t)len);
	}

	// The RTP header Marker bit MUST be set equal to 0 if the packet is not the last 
	// packet of the temporal unit, it SHOULD be set equal to 1 otherwise.
	// Note: It is possible for a receiver to receive the last packet of a temporal unit 
	// without the marker bit being set equal to 1, and a receiver should be able to handle 
	// this case. The last packet of a temporal unit is also indicated by the next packet, 
	// in RTP sequence number order, having an incremented timestamp.
	packer->pkt.rtp.m = 1;
	return rtp_av1_pack_flush(packer, packer->aggregation);
}

struct rtp_payload_encode_t *rtp_av1_encode()
{
	static struct rtp_payload_encode_t encode = {
		rtp_av1_pack_create,
		rtp_av1_pack_destroy,
		rtp_av1_pack_get_info,
		rtp_av1_pack_input_obu,
	};

	return &encode;
}
