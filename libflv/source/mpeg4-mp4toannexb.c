// ISO/IEC 14496-1:2010(E)
// Annex I: Usage of ITU-T Recommendation H.264 | ISO/IEC 14496-10 AVC (p150)

#include "mpeg4-avc.h"
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>

#define H264_NAL_IDR		5 // Coded slice of an IDR picture
#define H264_NAL_SPS		7 // Sequence parameter set
#define H264_NAL_PPS		8 // Picture parameter set
#define H264_NAL_AUD		9 // Access unit delimiter

struct h264_mp4toannexb_handle_t
{
	const struct mpeg4_avc_t* avc;
	int sps_pps_flag;
	int errcode;

	uint8_t* out;
	size_t bytes;
	size_t capacity;
};

static int h264_sps_pps_size(const struct mpeg4_avc_t* avc)
{
	int i, n = 0;
	for (i = 0; i < avc->nb_sps; i++)
		n += avc->sps[i].bytes + 4;
	for (i = 0; i < avc->nb_pps; i++)
		n += avc->pps[i].bytes + 4;
	return n;
}

static void h264_mp4toannexb_handler(void* param, const uint8_t* nalu, size_t bytes)
{
	int n;
	const uint8_t h264_start_code[] = { 0x00, 0x00, 0x00, 0x01 };
	struct h264_mp4toannexb_handle_t* mp4;
	mp4 = (struct h264_mp4toannexb_handle_t*)param;

	if (bytes < 1)
	{
		assert(0);
		mp4->errcode = -EINVAL;
		return;
	}

	// insert SPS/PPS before IDR frame
	switch (nalu[0] & 0x1f)
	{
	case H264_NAL_SPS:
	case H264_NAL_PPS:
		//flv->data[k++] = 0; // SPS/PPS add zero_byte(ITU H.264 B.1.2 Byte stream NAL unit semantics)
		mp4->sps_pps_flag = 1;
		break;

	case H264_NAL_IDR:
		if (0 == mp4->sps_pps_flag)
		{
			if (mp4->bytes > 0)
			{
				// write sps/pps at first
				n = h264_sps_pps_size(mp4->avc);
				if (n + mp4->bytes > mp4->capacity)
				{
					mp4->errcode = -E2BIG;
					return;
				}
				memmove(mp4->out + n, mp4->out, mp4->bytes);
			}
			n = mpeg4_avc_to_nalu(mp4->avc, mp4->out, mp4->capacity);
			if (n <= 0)
			{
				mp4->errcode = 0 == n ? -EINVAL : n;
				return;
			}
			mp4->bytes += n;
			mp4->sps_pps_flag = 1; // don't insert more than one-times
		}
		break;

#if defined(H2645_FILTER_AUD)
	case H264_NAL_AUD:
		continue; // ignore AUD
#endif
	}

	if (mp4->bytes + bytes + sizeof(h264_start_code) > mp4->capacity)
	{
		mp4->errcode = -E2BIG;
		return;
	}

	memcpy(mp4->out + mp4->bytes, h264_start_code, sizeof(h264_start_code));
	memcpy(mp4->out + mp4->bytes + sizeof(h264_start_code), nalu, bytes);
	mp4->bytes += sizeof(h264_start_code) + bytes;
}

int h264_mp4toannexb(const struct mpeg4_avc_t* avc, const void* data, size_t bytes, void* out, size_t size)
{
	int i, n;
	const uint8_t* src, *end;
	struct h264_mp4toannexb_handle_t h;

	memset(&h, 0, sizeof(h));
	h.avc = avc;
	h.out = (uint8_t*)out;
	h.capacity = size;

	end = (const uint8_t*)data + bytes;
	for (src = (const uint8_t*)data; src + avc->nalu < end; src += n + avc->nalu)
	{
		for (n = i = 0; i < avc->nalu; i++)
			n = (n << 8) + ((uint8_t*)src)[i];

		// fix 0x00 00 00 01 => flv nalu size
		if (0 == avc->nalu || (1 == n && (3 == avc->nalu || 4 == avc->nalu)))
		{
			//n = (int)(end - src) - avc->nalu;
			mpeg4_h264_annexb_nalu(src, end - src, h264_mp4toannexb_handler, &h);
			src = end;
			break;
		}

		if (n <= 0 || src + n + avc->nalu > end)
		{
			assert(0);
			return -EINVAL;
		}

		h264_mp4toannexb_handler(&h, src + avc->nalu, n);
	}

	assert(src == end);
	return 0 == h.errcode ? (int)h.bytes : 0;
}

#if defined(DEBUG) || defined(_DEBUG)
void h264_mp4toannexb_test(void)
{
	const uint8_t data[] = {
		0x01, 0x42, 0xe0, 0x1e, 0xff, 0xe1, 0x00, 0x21, 0x67, 0x42, 0xe0, 0x1e, 0xab, 0x40, 0xf0, 0x28,
		0xd0, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x19, 0x70, 0x20, 0x00, 0x78, 0x00, 0x00, 0x0f,
		0x00, 0x16, 0xb1, 0xb0, 0x3c, 0x50, 0xaa, 0x80, 0x80, 0x01, 0x00, 0x04, 0x28, 0xce, 0x3c, 0x80,
	};

	const uint8_t mp4[] = {
		0x00, 0x00, 0x00, 0x08, 0x65, 0x88, 0x84, 0x01, 0x7f, 0xec, 0x05, 0x17, 0x00, 0x00, 0x00, 0x01, 0xab,
	};

	const uint8_t annexb[] = {
		0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0xe0, 0x1e, 0xab, 0x40, 0xf0, 0x28, 0xd0, 0x80, 0x00, 0x00,
		0x00, 0x80, 0x00, 0x00, 0x19, 0x70, 0x20, 0x00, 0x78, 0x00, 0x00, 0x0f, 0x00, 0x16, 0xb1, 0xb0,
		0x3c, 0x50, 0xaa, 0x80, 0x80,
		0x00, 0x00, 0x00, 0x01, 0x28, 0xce, 0x3c, 0x80,
		0x00, 0x00, 0x00, 0x01, 0x65, 0x88, 0x84, 0x01, 0x7f, 0xec, 0x05, 0x17, 0x00, 0x00, 0x00, 0x01, 0xab,
	};

	int n;
	uint8_t out[sizeof(annexb) + 64];
	struct mpeg4_avc_t avc;
	memset(&avc, 0, sizeof(avc));
	assert(sizeof(data) == mpeg4_avc_decoder_configuration_record_load(data, sizeof(data), &avc));
	n = h264_mp4toannexb(&avc, mp4, sizeof(mp4), out, sizeof(out));
	assert(n == sizeof(annexb) && 0 == memcmp(annexb, out, n));
}
#endif