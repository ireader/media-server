#include "mpeg4-vvc.h"
#include "mpeg4-avc.h"
#include <string.h>
#include <assert.h>
#include <errno.h>

#define H266_NAL_IDR_W_RADL	7
#define H266_NAL_RSV_IRAP	11
#define H266_NAL_OPI		12
#define H266_NAL_DCI		13
#define H266_NAL_VPS		14
#define H266_NAL_SPS		15
#define H266_NAL_PPS		16
#define H266_NAL_PREFIX_APS	17
#define H266_NAL_SUFFIX_APS	18
#define H266_NAL_PH			19
#define H266_NAL_AUD		20
#define H266_NAL_PREFIX_SEI 23
#define H266_NAL_SUFFIX_SEI 24

#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define BIT(ptr, off) (((ptr)[(off) / 8] >> (7 - ((off) % 8))) & 0x01)

struct h266_annexbtomp4_handle_t
{
	struct mpeg4_vvc_t* vvc;
	int errcode;
	int* update; // avc sps/pps update flags
	int* vcl;

	uint8_t* out;
	size_t bytes;
	size_t capacity;
};

uint8_t mpeg4_h264_read_ue(const uint8_t* data, size_t bytes, size_t* offset);

static size_t vvc_rbsp_decode(const uint8_t* nalu, size_t bytes, uint8_t* sodb, size_t len)
{
	size_t i, j;
	const size_t max_sps_luma_bit_depth_offset = 256;
	for (j = i = 0; i < bytes && j < len && i < max_sps_luma_bit_depth_offset; i++)
	{
		if (i + 2 < bytes && 0 == nalu[i] && 0 == nalu[i + 1] && 0x03 == nalu[i + 2])
		{
			sodb[j++] = nalu[i];
			sodb[j++] = nalu[i + 1];
			i += 2;
		}
		else
		{
			sodb[j++] = nalu[i];
		}
	}
	return j;
}

static uint8_t vvc_vps_id(const uint8_t* rbsp, size_t bytes, struct mpeg4_vvc_t* vvc, uint8_t* ptr, size_t len)
{
	size_t sodb;
	uint8_t vps;
	uint8_t vps_max_layers_minus1;
	uint8_t vps_max_sub_layers_minus1;

	sodb = vvc_rbsp_decode(rbsp, bytes, ptr, len);
	if (sodb < 4)
		return 0xFF;

	vps = ptr[2] >> 4;  // 2-nalu type
	vps_max_layers_minus1 = (ptr[3] >> 2) & 0x3F;
	vps_max_sub_layers_minus1 = ((ptr[3] & 0x3) << 2) | ((ptr[4] >> 7) & 0x01);

	return vps;
}

static uint8_t vvc_sps_id(const uint8_t* rbsp, size_t bytes, struct mpeg4_vvc_t* vvc, uint8_t* ptr, size_t len, uint8_t* vps)
{
	size_t sodb;
	uint8_t sps;
	uint8_t sps_max_sub_layers_minus1;

	sodb = vvc_rbsp_decode(rbsp, bytes, ptr, len);
	if (sodb < 12 + 3)
		return 0xFF;

	sps = (ptr[2] >> 4) & 0x0F;  // 2-nalu type
	*vps = ptr[2] & 0x0F;
	sps_max_sub_layers_minus1 = (ptr[3] >> 5) & 0x07;
	vvc->chroma_format_idc = (ptr[3] >> 3) & 0x03;

	return sps;
}

static uint8_t vvc_pps_id(const uint8_t* rbsp, size_t bytes, struct mpeg4_vvc_t* vvc, uint8_t* ptr, size_t len, uint8_t* sps)
{
	uint8_t pps;
	size_t sodb;
	size_t n = 2 * 8;  // 2-nalu type
	sodb = vvc_rbsp_decode(rbsp, bytes, ptr, len);
	if (sodb < 12)
		return 0xFF; (void)vvc;
	pps = (ptr[2] >> 2) & 0x3F;  // 2-nalu type
	*sps = ((ptr[2] & 0x03) << 2) | ((ptr[3] >> 6) & 0x03);

	n = 11;
	vvc->max_picture_width = mpeg4_h264_read_ue(ptr, sodb, &n); // pic_width_in_luma_samples
	vvc->max_picture_height = mpeg4_h264_read_ue(ptr, sodb, &n); // pic_height_in_luma_samples
	return pps;
}

static void mpeg4_vvc_remove(struct mpeg4_vvc_t* vvc, uint8_t* ptr, size_t bytes, const uint8_t* end)
{
	uint8_t i;
	assert(ptr >= vvc->data && ptr + bytes <= end && end <= vvc->data + sizeof(vvc->data));
	memmove(ptr, ptr + bytes, end - ptr - bytes);

	for (i = 0; i < vvc->numOfArrays; i++)
	{
		if (vvc->nalu[i].data > ptr)
			vvc->nalu[i].data -= bytes;
	}
}

static int mpeg4_vvc_update2(struct mpeg4_vvc_t* vvc, int i, const uint8_t* nalu, size_t bytes)
{
	if (bytes == vvc->nalu[i].bytes && 0 == memcmp(nalu, vvc->nalu[i].data, bytes))
		return 0; // do nothing

	if (bytes > vvc->nalu[i].bytes && vvc->off + (bytes - vvc->nalu[i].bytes) > sizeof(vvc->data))
	{
		assert(0);
		return -1; // too big
	}

	mpeg4_vvc_remove(vvc, vvc->nalu[i].data, vvc->nalu[i].bytes, vvc->data + vvc->off);
	vvc->off -= vvc->nalu[i].bytes;

	vvc->nalu[i].data = vvc->data + vvc->off;
	vvc->nalu[i].bytes = (uint16_t)bytes;
	memcpy(vvc->nalu[i].data, nalu, bytes);
	vvc->off += bytes;
	return 1;
}

static int mpeg4_vvc_add(struct mpeg4_vvc_t* vvc, uint8_t type, const uint8_t* nalu, size_t bytes)
{
	// copy new
	assert(vvc->numOfArrays < sizeof(vvc->nalu) / sizeof(vvc->nalu[0]));
	if (vvc->numOfArrays >= sizeof(vvc->nalu) / sizeof(vvc->nalu[0])
		|| vvc->off + bytes > sizeof(vvc->data))
	{
		assert(0);
		return -1;
	}

	vvc->nalu[vvc->numOfArrays].type = type;
	vvc->nalu[vvc->numOfArrays].bytes = (uint16_t)bytes;
	vvc->nalu[vvc->numOfArrays].array_completeness = 1;
	vvc->nalu[vvc->numOfArrays].data = vvc->data + vvc->off;
	memcpy(vvc->nalu[vvc->numOfArrays].data, nalu, bytes);
	vvc->off += bytes;
	++vvc->numOfArrays;
	return 1;
}

static int h266_opi_copy(struct mpeg4_vvc_t* vvc, const uint8_t* nalu, size_t bytes)
{
	int i;
	for (i = 0; i < vvc->numOfArrays; i++)
	{
		if (H266_NAL_OPI == vvc->nalu[i].type)
			return mpeg4_vvc_update2(vvc, i, nalu, bytes);
	}

	return mpeg4_vvc_add(vvc, H266_NAL_OPI, nalu, bytes);
}

static int h266_dci_copy(struct mpeg4_vvc_t* vvc, const uint8_t* nalu, size_t bytes)
{
	int i;
	for (i = 0; i < vvc->numOfArrays; i++)
	{
		if (H266_NAL_DCI == vvc->nalu[i].type)
			return mpeg4_vvc_update2(vvc, i, nalu, bytes);
	}

	return mpeg4_vvc_add(vvc, H266_NAL_DCI, nalu, bytes);
}

static int h266_vps_copy(struct mpeg4_vvc_t* vvc, const uint8_t* nalu, size_t bytes)
{
	int i;
	uint8_t vpsid;

	if (bytes < 3)
	{
		assert(0);
		return -1; // invalid length
	}

	vpsid = vvc_vps_id(nalu, bytes, vvc, vvc->data + vvc->off, sizeof(vvc->data) - vvc->off);
	for (i = 0; i < vvc->numOfArrays; i++)
	{
		if (H266_NAL_VPS == vvc->nalu[i].type && vpsid == vvc_vps_id(vvc->nalu[i].data, vvc->nalu[i].bytes, vvc, vvc->data + vvc->off, sizeof(vvc->data) - vvc->off))
			return mpeg4_vvc_update2(vvc, i, nalu, bytes);
	}

	return mpeg4_vvc_add(vvc, H266_NAL_VPS, nalu, bytes);
}

static int h266_sps_copy(struct mpeg4_vvc_t* vvc, const uint8_t* nalu, size_t bytes)
{
	int i;
	uint8_t spsid;
	uint8_t vpsid, vpsid2;

	if (bytes < 13 + 2)
	{
		assert(0);
		return -1; // invalid length
	}

	spsid = vvc_sps_id(nalu, bytes, vvc, vvc->data + vvc->off, sizeof(vvc->data) - vvc->off, &vpsid);
	for (i = 0; i < vvc->numOfArrays; i++)
	{
		if (H266_NAL_SPS == vvc->nalu[i].type && spsid == vvc_sps_id(vvc->nalu[i].data, vvc->nalu[i].bytes, vvc, vvc->data + vvc->off, sizeof(vvc->data) - vvc->off, &vpsid2) && vpsid == vpsid2)
			return mpeg4_vvc_update2(vvc, i, nalu, bytes);
	}

	return mpeg4_vvc_add(vvc, H266_NAL_SPS, nalu, bytes);
}

static int h266_pps_copy(struct mpeg4_vvc_t* vvc, const uint8_t* nalu, size_t bytes)
{
	int i;
	uint8_t ppsid;
	uint8_t spsid, spsid2;

	if (bytes < 1 + 2)
	{
		assert(0);
		return -1; // invalid length
	}

	ppsid = vvc_pps_id(nalu, bytes, vvc, vvc->data + vvc->off, sizeof(vvc->data) - vvc->off, &spsid);
	for (i = 0; i < vvc->numOfArrays; i++)
	{
		if (H266_NAL_PPS == vvc->nalu[i].type && ppsid == vvc_pps_id(vvc->nalu[i].data, vvc->nalu[i].bytes, vvc, vvc->data + vvc->off, sizeof(vvc->data) - vvc->off, &spsid2) && spsid == spsid2)
			return mpeg4_vvc_update2(vvc, i, nalu, bytes);
	}

	return mpeg4_vvc_add(vvc, H266_NAL_PPS, nalu, bytes);
}

static int h266_sei_clear(struct mpeg4_vvc_t* vvc)
{
	int i;
	for (i = 0; i < vvc->numOfArrays; i++)
	{
		if (H266_NAL_PREFIX_SEI == vvc->nalu[i].type || H266_NAL_SUFFIX_SEI == vvc->nalu[i].type)
		{
			mpeg4_vvc_remove(vvc, vvc->nalu[i].data, vvc->nalu[i].bytes, vvc->data + vvc->off);
			vvc->off -= vvc->nalu[i].bytes;
			if (i + 1 < vvc->numOfArrays)
				memmove(vvc->nalu + i, vvc->nalu + i + 1, sizeof(vvc->nalu[0]) * (vvc->numOfArrays - i - 1));
			--vvc->numOfArrays;
			--i;
		}
	}
	return 0;
}

int mpeg4_vvc_update(struct mpeg4_vvc_t* vvc, const uint8_t* nalu, size_t bytes)
{
	int r;

	switch ((nalu[1] >> 3) & 0x1f)
	{
	case H266_NAL_OPI:
		r = h266_opi_copy(vvc, nalu, bytes);
		break;

	case H266_NAL_DCI:
		r = h266_dci_copy(vvc, nalu, bytes);
		break;

	case H266_NAL_VPS:
		h266_sei_clear(vvc); // remove all prefix/suffix sei
		r = h266_vps_copy(vvc, nalu, bytes);
		break;

	case H266_NAL_SPS:
		r = h266_sps_copy(vvc, nalu, bytes);
		break;

	case H266_NAL_PPS:
		r = h266_pps_copy(vvc, nalu, bytes);
		break;

#if defined(H266_FILTER_SEI)
	case H266_NAL_PREFIX_SEI:
		r = mpeg4_vvc_add(vvc, H266_NAL_SEI_PREFIX, nalu, bytes);
		break;

	case H266_NAL_SUFFIX_SEI:
		r = mpeg4_vvc_add(vvc, H266_NAL_SEI_SUFFIX, nalu, bytes);
		break;
#endif

	default:
		r = 0;
		break;
	}

	return r;
}

static void vvc_handler(void* param, const uint8_t* nalu, size_t bytes)
{
	int r;
	uint8_t nalutype;
	struct h266_annexbtomp4_handle_t* mp4;
	mp4 = (struct h266_annexbtomp4_handle_t*)param;

	if (bytes < 2)
	{
		assert(0);
		mp4->errcode = -EINVAL;
		return;
	}

	nalutype = (nalu[1] >> 3) & 0x1f;
#if defined(H2645_FILTER_AUD)
	if (H266_NAL_AUD == nalutype)
		return; // ignore AUD
#endif

	r = mpeg4_vvc_update(mp4->vvc, nalu, bytes);
	if (1 == r && mp4->update)
		*mp4->update = 1;
	else if (r < 0)
		mp4->errcode = r;

	// IRAP-1, B/P-2, other-0
	if (mp4->vcl && nalutype < H266_NAL_OPI)
		*mp4->vcl = H266_NAL_IDR_W_RADL <= nalutype && nalutype <= H266_NAL_RSV_IRAP ? 1 : 2;

	if (mp4->capacity >= mp4->bytes + bytes + 4)
	{
		mp4->out[mp4->bytes + 0] = (uint8_t)((bytes >> 24) & 0xFF);
		mp4->out[mp4->bytes + 1] = (uint8_t)((bytes >> 16) & 0xFF);
		mp4->out[mp4->bytes + 2] = (uint8_t)((bytes >> 8) & 0xFF);
		mp4->out[mp4->bytes + 3] = (uint8_t)((bytes >> 0) & 0xFF);
		memmove(mp4->out + mp4->bytes + 4, nalu, bytes);
		mp4->bytes += bytes + 4;
	}
	else
	{
		mp4->errcode = -1;
	}
}

int h266_annexbtomp4(struct mpeg4_vvc_t* vvc, const void* data, size_t bytes, void* out, size_t size, int* vcl, int* update)
{
	struct h266_annexbtomp4_handle_t h;
	memset(&h, 0, sizeof(h));
	h.vvc = vvc;
	h.vcl = vcl;
	h.update = update;
	h.out = (uint8_t*)out;
	h.capacity = size;
	if (vcl) *vcl = 0;
	if (update) *update = 0;

	//	vvc->numTemporalLayers = 0;
	//	vvc->temporalIdNested = 0;
	//	vvc->min_spatial_segmentation_idc = 0;
	//	vvc->general_profile_compatibility_flags = 0xffffffff;
	//	vvc->general_constraint_indicator_flags = 0xffffffffffULL;
	//	vvc->chromaFormat = 1; // 4:2:0

	mpeg4_h264_annexb_nalu((const uint8_t*)data, bytes, vvc_handler, &h);
	vvc->lengthSizeMinusOne = 3; // 4 bytes
	return 0 == h.errcode ? (int)h.bytes : 0;
}

int h266_is_new_access_unit(const uint8_t* nalu, size_t bytes)
{
	uint8_t nal_type;
	uint8_t nuh_layer_id;

	if (bytes < 3)
		return 0;

	nal_type = (nalu[1] >> 3) & 0x1f;
	nuh_layer_id = nalu[0] & 0x3F;

	// 7.4.2.4.3 Order of PUs and their association to AUs
	if (H266_NAL_AUD == nal_type || H266_NAL_OPI == nal_type || H266_NAL_DCI == nal_type || H266_NAL_VPS == nal_type || H266_NAL_SPS == nal_type || H266_NAL_PPS == nal_type ||
		(nuh_layer_id == 0 && (H266_NAL_PREFIX_APS == nal_type || H266_NAL_PH == nal_type || H266_NAL_PREFIX_SEI == nal_type ||
			26 == nal_type || (28 <= nal_type && nal_type <= 29))))
		return 1;

	// 7.4.2.4.4 Order of NAL units and coded pictures and their association to PUs
	if (nal_type < H266_NAL_OPI)
	{
		//sh_picture_header_in_slice_header_flag == 1
		return (nalu[2] & 0x80) ? 1 : 0;
	}

	return 0;
}

#if defined(_DEBUG) || defined(DEBUG)
void vvc_annexbtomp4_test(void)
{
	const uint8_t vps[] = { 0x40, 0x01, 0x0c, 0x01, 0xff, 0xff, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00, 0x80, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x78, 0x9d, 0xc0, 0x90 };
	const uint8_t sps[] = { 0x42, 0x01, 0x01, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00, 0x80, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x78, 0xa0, 0x03, 0xc0, 0x80, 0x32, 0x16, 0x59, 0xde, 0x49, 0x1b, 0x6b, 0x80, 0x40, 0x00, 0x00, 0xfa, 0x00, 0x00, 0x17, 0x70, 0x02 };
	const uint8_t pps[] = { 0x44, 0x01, 0xc1, 0x73, 0xd1, 0x89 };
	const uint8_t annexb[] = { 0x00, 0x00, 0x00, 0x01, 0x4e, 0x01, 0x06, 0x01, 0xd0, 0x80, 0x00, 0x00, 0x00, 0x01, 0x40, 0x01, 0x0c, 0x01, 0xff, 0xff, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00, 0x80, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x78, 0x9d, 0xc0, 0x90, 0x00, 0x00, 0x00, 0x01, 0x42, 0x01, 0x01, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00, 0x80, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x78, 0xa0, 0x03, 0xc0, 0x80, 0x32, 0x16, 0x59, 0xde, 0x49, 0x1b, 0x6b, 0x80, 0x40, 0x00, 0x00, 0xfa, 0x00, 0x00, 0x17, 0x70, 0x02, 0x00, 0x00, 0x00, 0x01, 0x44, 0x01, 0xc1, 0x73, 0xd1, 0x89 };
	uint8_t output[512];
	int vcl, update;

	struct mpeg4_vvc_t vvc;
	memset(&vvc, 0, sizeof(vvc));
	assert(h266_annexbtomp4(&vvc, annexb, sizeof(annexb), output, sizeof(output), &vcl, &update) > 0);
	assert(3 == vvc.numOfArrays && vcl == 0 && update == 1);
	assert(vvc.nalu[0].bytes == sizeof(vps) && 0 == memcmp(vvc.nalu[0].data, vps, sizeof(vps)));
	assert(vvc.nalu[1].bytes == sizeof(sps) && 0 == memcmp(vvc.nalu[1].data, sps, sizeof(sps)));
	assert(vvc.nalu[2].bytes == sizeof(pps) && 0 == memcmp(vvc.nalu[2].data, pps, sizeof(pps)));
}
#endif
