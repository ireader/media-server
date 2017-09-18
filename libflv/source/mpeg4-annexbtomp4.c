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

static void h264_handler(void* param, const void* nalu, size_t bytes)
{
	struct mpeg4_handle_t* mp4;	
	mp4 = (struct mpeg4_handle_t*)param;

	switch (((unsigned char*)nalu)[0] & 0x1f)
	{
	case H264_NAL_SPS:
		assert(bytes <= sizeof(mp4->avc->sps[mp4->avc->nb_sps].data));
		if (bytes <= sizeof(mp4->avc->sps[mp4->avc->nb_sps].data)
			&& mp4->avc->nb_sps < sizeof(mp4->avc->sps) / sizeof(mp4->avc->sps[0]))
		{
			mp4->avc->sps[mp4->avc->nb_sps].bytes = (uint16_t)bytes;
			memcpy(mp4->avc->sps[mp4->avc->nb_sps].data, nalu, bytes);
			++mp4->avc->nb_sps;
		}
		else
		{
			mp4->errcode = -1;
		}

		if (1 == mp4->avc->nb_sps)
		{
			mp4->avc->profile = mp4->avc->sps[0].data[1];
			mp4->avc->compatibility = mp4->avc->sps[0].data[2];
			mp4->avc->level = mp4->avc->sps[0].data[3];
		}
		break;

	case H264_NAL_PPS:
		assert(bytes <= sizeof(mp4->avc->pps[mp4->avc->nb_pps].data));
		if (bytes <= sizeof(mp4->avc->pps[mp4->avc->nb_pps].data)
			&& mp4->avc->nb_pps < sizeof(mp4->avc->pps) / sizeof(mp4->avc->pps[0]))
		{
			mp4->avc->pps[mp4->avc->nb_pps].bytes = (uint16_t)bytes;
			memcpy(mp4->avc->pps[mp4->avc->nb_pps].data, nalu, bytes);
			++mp4->avc->nb_pps;
		}
		else
		{
			mp4->errcode = -1;
		}
		break;

	case H264_NAL_IDR:
		mp4->avc->chroma_format_idc = 1;
		break;

	//case H264_NAL_SPS_EXTENSION:
	//case H264_NAL_SPS_SUBSET:
	//	break;
	}

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
