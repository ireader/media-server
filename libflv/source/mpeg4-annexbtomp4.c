// ISO/IEC 14496-1:2010(E)
// Annex I: Usage of ITU-T Recommendation H.264 | ISO/IEC 14496-10 AVC (p150)
//
// 1. Start Codes shall not be present in the stream. The field indicating the size of each following NAL unit
//    shall be added before NAL unit.The size of this field is defined in DecoderSpecificInfo.
// 2. It is recommended encapsulating one NAL unit in one SL packet when it is delivered over lossy environment.

#include "mpeg4-avc.h"
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stddef.h>

#define H264_NAL_IDR		5 // Coded slice of an IDR picture
#define H264_NAL_SPS		7 // Sequence parameter set
#define H264_NAL_PPS		8 // Picture parameter set

struct mpeg4_handle_t
{
	struct mpeg4_avc_t* avc;
	int errcode;

	uint8_t* ptr;
	size_t bytes;
	size_t capacity;
};

typedef void(*h264_nalu_handler)(void* param, const void* nalu, size_t bytes);

static const uint8_t* h264_startcode(const uint8_t *data, size_t bytes)
{
	size_t i;
	for (i = 2; i + 1 < bytes; i++)
	{
		if (0x01 == data[i] && 0x00 == data[i - 1] && 0x00 == data[i - 2])
			return data + i + 1;
	}

	return NULL;
}

///@param[in] h264 H.264 byte stream format data(A set of NAL units)
static void h264_stream(const void* h264, size_t bytes, h264_nalu_handler handler, void* param)
{
	ptrdiff_t n;
	const unsigned char* p, *next, *end;

	end = (const unsigned char*)h264 + bytes;
	p = h264_startcode((const unsigned char*)h264, bytes);

	while (p)
	{
		next = h264_startcode(p, end - p);
		if (next)
		{
			n = next - p - 3;
		}
		else
		{
			n = end - p;
		}

		while (n > 0 && 0 == p[n - 1]) n--; // filter tailing zero

		assert(n > 0);
		if (n > 0)
		{
			handler(param, p, (size_t)n);
		}

		p = next;
	}
}

static uint8_t h264_read_ue(const uint8_t* data, size_t bytes)
{
	int bit, i;
	int leadingZeroBits = -1;
	size_t offset = 0;

	for (bit = 0; !bit && offset / 8 < bytes; ++leadingZeroBits)
	{
		bit = (data[offset / 8] >> (7 - (offset % 8))) & 0x01;
		++offset;
	}

	bit = 0;
	assert(leadingZeroBits < 32);
	for(i = 0; i < leadingZeroBits && offset / 8 < bytes; i++)
	{
		bit = (bit << 1) | (data[offset / 8] >> (7 - (offset % 8))) & 0x01;
		++offset;
	}

	return (uint8_t)((1 << leadingZeroBits) - 1 + bit);
}

static inline uint8_t h264_sps_id(const uint8_t* data, size_t bytes)
{
	if (bytes < 4)
		return 0;
	return h264_read_ue(data + 3, bytes - 3);
}

static inline uint8_t h264_pps_id(const uint8_t* data, size_t bytes)
{
	return h264_read_ue(data, bytes);
}

/// @return >0-ok(nalu type), <=0-error
int h264_update_avc(struct mpeg4_avc_t* avc, const uint8_t* nalu, size_t bytes)
{
	uint8_t id;
	uint8_t nalutype;
	uint8_t* avcptr;
	
	// get remain data
	avcptr = avc->nb_sps > 0 ? avc->sps[avc->nb_sps - 1].data + avc->sps[avc->nb_sps - 1].bytes : avc->data;
	if (avc->nb_pps > 0 && avc->pps[avc->nb_pps - 1].data + avc->pps[avc->nb_pps - 1].bytes > avcptr)
		avcptr = avc->pps[avc->nb_pps - 1].data + avc->pps[avc->nb_pps - 1].bytes;

	nalutype = nalu[0] & 0x1f;
	switch (nalutype)
	{
	case H264_NAL_SPS:
		id = h264_sps_id(nalu + 1, bytes - 1);
		if (avcptr + bytes > avc->data + sizeof(avc->data)
			|| id >= sizeof(avc->sps) / sizeof(avc->sps[0]))
		{
			assert(0);
			return -1;
		}

		assert(id >= avc->nb_sps || id == h264_sps_id(avc->sps[id].data + 1, avc->sps[id].bytes - 1));
		id = id >= avc->nb_sps ? avc->nb_sps++ : id;
		avc->sps[id].data = avcptr;
		avc->sps[id].bytes = (uint16_t)bytes;
		memcpy(avc->sps[id].data, nalu, bytes);

		if (1 == avc->nb_sps)
		{
			avc->profile = avc->sps[0].data[1];
			avc->compatibility = avc->sps[0].data[2];
			avc->level = avc->sps[0].data[3];
		}
		break;

	case H264_NAL_PPS:
		id = h264_pps_id(nalu + 1, bytes - 1);
		if (avcptr + bytes > avc->data + sizeof(avc->data)
			|| id >= sizeof(avc->pps) / sizeof(avc->pps[0]))
		{
			assert(0);
			return -1;
		}

		assert(id >= avc->nb_pps || id == h264_pps_id(avc->pps[id].data + 1, avc->pps[id].bytes - 1));
		id = id >= avc->nb_pps ? avc->nb_pps++ : id;
		avc->pps[id].data = avcptr;
		avc->pps[id].bytes = (uint16_t)bytes;
		memcpy(avc->pps[id].data, nalu, bytes);
		break;

	//case H264_NAL_IDR:
	//	break;
	//case H264_NAL_SPS_EXTENSION:
	//case H264_NAL_SPS_SUBSET:
	//	break;
	//default:
	//	break;
	}

	return nalutype;
}

static void h264_handler(void* param, const void* nalu, size_t bytes)
{
	int nalutype;
	struct mpeg4_handle_t* mp4;	
	mp4 = (struct mpeg4_handle_t*)param;

	nalutype = h264_update_avc(mp4->avc, (const uint8_t*)nalu, bytes);
	if (nalutype < 0)
	{
		mp4->errcode = -1;
		return;
	}

	// TODO: hack!!! idr flag
	if(H264_NAL_IDR == nalutype)
		mp4->avc->chroma_format_idc = 1;

	if (mp4->capacity >= mp4->bytes + bytes + 4)
	{
		mp4->ptr[mp4->bytes + 0] = (uint8_t)((bytes >> 24) & 0xFF);
		mp4->ptr[mp4->bytes + 1] = (uint8_t)((bytes >> 16) & 0xFF);
		mp4->ptr[mp4->bytes + 2] = (uint8_t)((bytes >> 8) & 0xFF);
		mp4->ptr[mp4->bytes + 3] = (uint8_t)((bytes >> 0) & 0xFF);
		memcpy(mp4->ptr + mp4->bytes + 4, nalu, bytes);
		mp4->bytes += bytes + 4;
	}
	else
	{
		mp4->errcode = -1;
	}
}

size_t mpeg4_annexbtomp4(struct mpeg4_avc_t* avc, const void* data, size_t bytes, void* out, size_t size)
{
	struct mpeg4_handle_t h;
	h.avc = avc;
	h.ptr = (uint8_t*)out;
	h.capacity = size;
	h.bytes = 0;
	h.errcode = 0;
	avc->chroma_format_idc = 0;
	avc->nb_pps = 0;
	avc->nb_sps = 0;
	avc->nalu = 4;
	h264_stream(data, bytes, h264_handler, &h);
	return 0 == h.errcode ? h.bytes : 0;
}
