#include "mpeg4-hevc.h"
#include "mpeg4-avc.h"
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>

#define H265_NAL_BLA_W_LP	16
#define H265_NAL_RSV_IRAP	23
#define H265_NAL_VPS		32
#define H265_NAL_SPS		33
#define H265_NAL_PPS		34
#define H265_NAL_AUD		35 // Access unit delimiter

struct h265_mp4toannexb_handle_t
{
	const struct mpeg4_hevc_t* hevc;
	int vps_sps_pps_flag;
	int errcode;

	uint8_t* out;
	size_t bytes;
	size_t capacity;
};

static int h265_vps_sps_pps_size(const struct mpeg4_hevc_t* hevc)
{
	int i, n = 0;
	for (i = 0; i < hevc->numOfArrays; i++)
		n += hevc->nalu[i].bytes + 4;
	return n;
}

static void h265_mp4toannexb_handler(void* param, const uint8_t* nalu, size_t bytes)
{
	int n;
	uint8_t irap, nalu_type;
	const uint8_t h265_start_code[] = { 0x00, 0x00, 0x00, 0x01 };
	struct h265_mp4toannexb_handle_t* mp4;
	mp4 = (struct h265_mp4toannexb_handle_t*)param;

	if (bytes < 1)
	{
		assert(0);
		mp4->errcode = -EINVAL;
		return;
	}

	nalu_type = (nalu[0] >> 1) & 0x3F;
#if defined(H2645_FILTER_AUD)
	if (H265_NAL_AUD == nalu_type)
		continue; // ignore AUD
#endif

	if (H265_NAL_VPS == nalu_type || H265_NAL_SPS == nalu_type || H265_NAL_PPS == nalu_type)
		mp4->vps_sps_pps_flag = 1;

	irap = H265_NAL_BLA_W_LP <= nalu_type && nalu_type <= H265_NAL_RSV_IRAP;
	if (irap && 0 == mp4->vps_sps_pps_flag)
	{
		// insert VPS/SPS/PPS before IDR frame
		if (mp4->bytes > 0)
		{
			// write sps/pps at first
			n = h265_vps_sps_pps_size(mp4->hevc);
			if (n + mp4->bytes > mp4->capacity)
			{
				mp4->errcode = -E2BIG;
				return;
			}
			memmove(mp4->out + n, mp4->out, mp4->bytes);
		}

		n = mpeg4_hevc_to_nalu(mp4->hevc, mp4->out, mp4->capacity);
		if (n <= 0)
		{
			mp4->errcode = 0 == n ? -EINVAL : n;
			return;
		}
		mp4->bytes += n;
		mp4->vps_sps_pps_flag = 1;
	}

	if (mp4->bytes + bytes + sizeof(h265_start_code) > mp4->capacity)
	{
		mp4->errcode = -E2BIG;
		return;
	}

	memcpy(mp4->out + mp4->bytes, h265_start_code, sizeof(h265_start_code));
	memcpy(mp4->out + mp4->bytes + sizeof(h265_start_code), nalu, bytes);
	mp4->bytes += sizeof(h265_start_code) + bytes;
}

int h265_mp4toannexb(const struct mpeg4_hevc_t* hevc, const void* data, size_t bytes, void* out, size_t size)
{
	int i, n;
	const uint8_t* src, * end;
	struct h265_mp4toannexb_handle_t h;

	memset(&h, 0, sizeof(h));
	h.hevc = hevc;
	h.out = (uint8_t*)out;
	h.capacity = size;

	end = (uint8_t*)data + bytes;
	for(src = (uint8_t*)data; src + hevc->lengthSizeMinusOne + 1 < end; src += n)
	{
		for (n = i = 0; i < hevc->lengthSizeMinusOne + 1; i++)
			n = (n << 8) + ((uint8_t*)src)[i];
		
		// fix 0x00 00 00 01 => flv nalu size
		if (0 == hevc->lengthSizeMinusOne || (1 == n && (2 == hevc->lengthSizeMinusOne || 3 == hevc->lengthSizeMinusOne)))
		{
			//n = (int)(end - src) - avc->nalu;
			mpeg4_h264_annexb_nalu(src, end - src, h265_mp4toannexb_handler, &h);
			src = end;
			break;
		}

		src += hevc->lengthSizeMinusOne + 1;
		if (n < 1 || src + n > end)
		{
			assert(0);
			return -EINVAL;
		}

		h265_mp4toannexb_handler(&h, src, n);
	}

	assert(src == end);
	return 0 == h.errcode ? (int)h.bytes : 0;
}
