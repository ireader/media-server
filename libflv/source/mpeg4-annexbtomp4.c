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
#define H264_NAL_AUD		9 // Access unit delimiter

struct h264_annexbtomp4_handle_t
{
	struct mpeg4_avc_t* avc;
	uint8_t* avcptr;
	int errcode;
	int* vcl;

	uint8_t* ptr;
	size_t bytes;
	size_t capacity;
};

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
void h264_annexb_nalu(const void* h264, size_t bytes, void (*handler)(void* param, const void* nalu, size_t bytes), void* param)
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

static void mpeg4_avc_clear(struct h264_annexbtomp4_handle_t* mp4)
{
	if (NULL != mp4->avcptr)
		return;
	memset(mp4->avc, 0, sizeof(*mp4->avc));
	mp4->avcptr = mp4->avc->data;
	mp4->avc->nb_sps = 0;
	mp4->avc->nb_pps = 0;
}

static void h264_handler(void* param, const void* nalu, size_t bytes)
{
	int nalutype;
	struct h264_annexbtomp4_handle_t* mp4;	
	mp4 = (struct h264_annexbtomp4_handle_t*)param;

	if (bytes < 1)
	{
		assert(0);
		return;
	}

	nalutype = (*(uint8_t*)nalu) & 0x1f;
	switch (nalutype)
	{
	case H264_NAL_SPS:
		mpeg4_avc_clear(mp4);
		assert(mp4->avc->nb_sps < sizeof(mp4->avc->sps) / sizeof(mp4->avc->sps[0]));
		if (mp4->avc->nb_sps >= sizeof(mp4->avc->sps) / sizeof(mp4->avc->sps[0])
			|| mp4->avcptr + bytes > mp4->avc->data + sizeof(mp4->avc->data))
		{
			assert(0);
			mp4->errcode = -1;
			return;
		}

		mp4->avc->profile = ((uint8_t*)nalu)[1];
		mp4->avc->compatibility = ((uint8_t*)nalu)[2];
		mp4->avc->level = ((uint8_t*)nalu)[3];

		mp4->avc->sps[mp4->avc->nb_sps].data = mp4->avcptr;
		mp4->avc->sps[mp4->avc->nb_sps].bytes = (uint16_t)bytes;
		memcpy(mp4->avcptr, nalu, bytes);
		mp4->avcptr += bytes;
		++mp4->avc->nb_sps;
		return;

	case H264_NAL_PPS:
		mpeg4_avc_clear(mp4);
		assert(mp4->avc->nb_pps < sizeof(mp4->avc->pps) / sizeof(mp4->avc->pps[0]));
		if (mp4->avc->nb_pps >= sizeof(mp4->avc->pps) / sizeof(mp4->avc->pps[0])
			|| mp4->avcptr + bytes > mp4->avc->data + sizeof(mp4->avc->data))
		{
			assert(0);
			mp4->errcode = -1;
			return;
		}

		mp4->avc->pps[mp4->avc->nb_pps].data = mp4->avcptr;
		mp4->avc->pps[mp4->avc->nb_pps].bytes = (uint16_t)bytes;
		memcpy(mp4->avcptr, nalu, bytes);
		mp4->avcptr += bytes;
		++mp4->avc->nb_pps;
		return;

#if defined(H2645_FILTER_AUD)
	case H264_NAL_AUD:
		return; // ignore AUD
#endif
	}

	// IDR-1, B/P-2, other-0
	if (mp4->vcl && 1 <= nalutype && nalutype <= H264_NAL_IDR)
		*mp4->vcl = nalutype == H264_NAL_IDR ? 1 : 2;

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

int h264_annexbtomp4(struct mpeg4_avc_t* avc, const void* data, int bytes, void* out, int size, int* vcl)
{
	struct h264_annexbtomp4_handle_t h;
	memset(&h, 0, sizeof(h));
	h.avc = avc;
	h.vcl = vcl;
	h.ptr = (uint8_t*)out;
	h.capacity = size;
	if (vcl) *vcl = 0;
	
	h264_annexb_nalu(data, bytes, h264_handler, &h);
	avc->nalu = 4;
	return 0 == h.errcode ? h.bytes : 0;
}

#if defined(_DEBUG) || defined(DEBUG)
void mpeg4_annexbtomp4_test(void)
{
	const uint8_t sps[] = { 0x67,0x42,0xe0,0x1e,0xab };
	const uint8_t pps[] = { 0x28,0xce,0x3c,0x80 };
	const uint8_t annexb[] = { 0x00,0x00,0x00,0x01,0x67,0x42,0xe0,0x1e,0xab, 0x00,0x00,0x00,0x01,0x28,0xce,0x3c,0x80,0x00,0x00,0x00,0x01,0x65,0x11 };
	uint8_t output[256];
	int vcl;

	struct mpeg4_avc_t avc;
	memset(&avc, 0, sizeof(avc));
	assert(h264_annexbtomp4(&avc, annexb, sizeof(annexb), output, sizeof(output), &vcl) > 0);
	assert(1 == avc.nb_sps && avc.sps[0].bytes == sizeof(sps) && 0 == memcpy(avc.sps[0].data, sps, sizeof(sps)));
	assert(1 == avc.nb_pps && avc.pps[0].bytes == sizeof(pps) && 0 == memcpy(avc.pps[0].data, pps, sizeof(pps)));
	assert(vcl == 1);
}
#endif
