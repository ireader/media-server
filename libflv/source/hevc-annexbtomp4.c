#include "mpeg4-hevc.h"
#include "mpeg4-avc.h"
#include <string.h>
#include <assert.h>

#define H265_NAL_VPS		32
#define H265_NAL_SPS		33
#define H265_NAL_PPS		34
#define H265_NAL_AUD		35
#define H265_NAL_SEI_PREFIX	39
#define H265_NAL_SEI_SUFFIX	40

#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define BIT(ptr, off) (((ptr)[(off) / 8] >> (7 - ((off) % 8))) & 0x01)

struct h265_annexbtomp4_handle_t
{
	struct mpeg4_hevc_t* hevc;
	uint8_t* hevcptr;
	int errcode;
	int* update; // avc sps/pps update flags
	int* vcl;

	uint8_t* out;
	int bytes;
	int capacity;
};

uint8_t mpeg4_h264_read_ue(const uint8_t* data, int bytes, int* offset);

static int hevc_rbsp_decode(const uint8_t* nalu, int bytes, uint8_t* sodb)
{
	int i, j;
	for (j = i = 0; i < bytes; i++)
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

static int hevc_profile_tier_level(const uint8_t* nalu, int bytes, uint8_t maxNumSubLayersMinus1, struct mpeg4_hevc_t* hevc)
{
	int n;
	uint8_t i;
	uint8_t sub_layer_profile_present_flag[8];
	uint8_t sub_layer_level_present_flag[8];

	if (bytes < 12)
		return -1;

	hevc->general_profile_space = (nalu[0] >> 6) & 0x03;
	hevc->general_tier_flag = (nalu[0] >> 5) & 0x01;
	hevc->general_profile_idc = nalu[0] & 0x1f;

	hevc->general_profile_compatibility_flags = 0;
	hevc->general_profile_compatibility_flags |= nalu[1] << 24;
	hevc->general_profile_compatibility_flags |= nalu[2] << 16;
	hevc->general_profile_compatibility_flags |= nalu[3] << 8;
	hevc->general_profile_compatibility_flags |= nalu[4];

	hevc->general_constraint_indicator_flags = 0;
	hevc->general_constraint_indicator_flags |= ((uint64_t)nalu[5]) << 40;
	hevc->general_constraint_indicator_flags |= ((uint64_t)nalu[6]) << 32;
	hevc->general_constraint_indicator_flags |= ((uint64_t)nalu[7]) << 24;
	hevc->general_constraint_indicator_flags |= ((uint64_t)nalu[8]) << 16;
	hevc->general_constraint_indicator_flags |= ((uint64_t)nalu[9]) << 8;
	hevc->general_constraint_indicator_flags |= nalu[10];

	hevc->general_level_idc = nalu[11];
	if (maxNumSubLayersMinus1 < 1)
		return 12;

	if (bytes < 14)
		return -1; // error

	for (i = 0; i < maxNumSubLayersMinus1; i++)
	{
		sub_layer_profile_present_flag[i] = BIT(nalu, 12 * 8 + i * 2);
		sub_layer_level_present_flag[i] = BIT(nalu, 12 * 8 + i * 2 + 1);
	}

	n = 12 + 2;
	for (i = 0; i < maxNumSubLayersMinus1; i++)
	{
		if(sub_layer_profile_present_flag[i])
			n += 11;
		if (sub_layer_level_present_flag[i])
			n += 1;
	}

	return bytes < n ? n : -1;
}

static uint8_t hevc_vps_id(const uint8_t* rbsp, int bytes, struct mpeg4_hevc_t* hevc, uint8_t* ptr)
{
	int sodb;
	uint8_t vps;
	uint8_t vps_max_sub_layers_minus1;
	uint8_t vps_temporal_id_nesting_flag;

	sodb = hevc_rbsp_decode(rbsp, bytes, ptr);
	if (sodb < 16 + 2)
		return 0xFF;

	vps = ptr[2] >> 4;  // 2-nalu type
	vps_max_sub_layers_minus1 = (ptr[3] >> 1) & 0x07;
	vps_temporal_id_nesting_flag = ptr[3] & 0x01;
	hevc->numTemporalLayers = MAX(hevc->numTemporalLayers, vps_max_sub_layers_minus1 + 1);
	hevc->temporalIdNested = (hevc->temporalIdNested || vps_temporal_id_nesting_flag) ? 1 : 0;
	hevc_profile_tier_level(ptr + 6, sodb - 6, vps_max_sub_layers_minus1, hevc);

	return vps;
}

static uint8_t hevc_sps_id(const uint8_t* rbsp, int bytes, struct mpeg4_hevc_t* hevc, uint8_t* ptr, uint8_t* vps)
{
	int n;
	int sodb;
	uint8_t sps;
	uint8_t sps_max_sub_layers_minus1;
	uint8_t sps_temporal_id_nesting_flag;
	uint8_t conformance_window_flag;

	sodb = hevc_rbsp_decode(rbsp, bytes, ptr);
	if (sodb < 12+3)
		return 0xFF;

	*vps = ptr[2] >> 4;  // 2-nalu type
	sps_max_sub_layers_minus1 = (ptr[2] >> 1) & 0x07;
	sps_temporal_id_nesting_flag = ptr[2] & 0x01;
	n = hevc_profile_tier_level(ptr + 3, sodb - 3, sps_max_sub_layers_minus1, hevc);
	if (n <= 0)
		return 0xFF;

	n = (n + 3) * 8;
	sps = mpeg4_h264_read_ue(ptr, sodb, &n);
	hevc->chromaFormat = mpeg4_h264_read_ue(ptr, sodb, &n);
	if (3 == hevc->chromaFormat)
		n++;
	mpeg4_h264_read_ue(ptr, sodb, &n); // pic_width_in_luma_samples
	mpeg4_h264_read_ue(ptr, sodb, &n); // pic_height_in_luma_samples
	conformance_window_flag = BIT(ptr, n); n++; // conformance_window_flag
	if (conformance_window_flag)
	{
		mpeg4_h264_read_ue(ptr, sodb, &n); // conf_win_left_offset
		mpeg4_h264_read_ue(ptr, sodb, &n); // conf_win_right_offset
		mpeg4_h264_read_ue(ptr, sodb, &n); // conf_win_top_offset
		mpeg4_h264_read_ue(ptr, sodb, &n); // conf_win_bottom_offset
	}
	hevc->bitDepthLumaMinus8 = mpeg4_h264_read_ue(ptr, sodb, &n);
	hevc->bitDepthChromaMinus8 = mpeg4_h264_read_ue(ptr, sodb, &n);

	// TODO: vui_parameters
	//mp4->hevc->min_spatial_segmentation_idc; // min_spatial_segmentation_idc
	return sps;
}

static uint8_t hevc_pps_id(const uint8_t* rbsp, int bytes, struct mpeg4_hevc_t* hevc, uint8_t* ptr, uint8_t* sps)
{
	// TODO:
	//mp4->hevc->parallelismType; // entropy_coding_sync_enabled_flag

	int sodb;
	int offset = 2 * 8;  // 2-nalu type
	sodb = hevc_rbsp_decode(rbsp, bytes, ptr);
	if (sodb < 3)
		return 0xFF;
	*sps = mpeg4_h264_read_ue(ptr, sodb, &offset);
	return mpeg4_h264_read_ue(ptr, sodb, &offset);
}

static void mpeg4_hevc_remove(struct mpeg4_hevc_t* hevc, uint8_t* ptr, int bytes, const uint8_t* end)
{
	uint8_t i;
	assert(ptr >= hevc->data && ptr + bytes <= end && end <= hevc->data + sizeof(hevc->data));
	memmove(ptr, ptr + bytes, end - ptr - bytes);

	for (i = 0; i < hevc->numOfArrays; i++)
	{
		if (hevc->nalu[i].data > ptr)
			hevc->nalu[i].data -= bytes;
	}
}

static int mpeg4_hevc_update(struct h265_annexbtomp4_handle_t* mp4, int i, const uint8_t* nalu, int bytes)
{
	if (bytes == mp4->hevc->nalu[i].bytes && 0 == memcmp(nalu, mp4->hevc->nalu[i].data, bytes))
		return 0; // do nothing

	if (bytes > mp4->hevc->nalu[i].bytes && mp4->hevcptr + (bytes - mp4->hevc->nalu[i].bytes) > mp4->hevc->data + sizeof(mp4->hevc->data))
	{
		assert(0);
		mp4->errcode = -1;
		return -1; // too big
	}

	mpeg4_hevc_remove(mp4->hevc, mp4->hevc->nalu[i].data, mp4->hevc->nalu[i].bytes, mp4->hevcptr);
	mp4->hevcptr -= mp4->hevc->nalu[i].bytes;

	if (mp4->update) *mp4->update = 1; // set update flag
	mp4->hevc->nalu[i].data = mp4->hevcptr;
	mp4->hevc->nalu[i].bytes = (uint16_t)bytes;
	memcpy(mp4->hevcptr, nalu, bytes);
	mp4->hevcptr += bytes;
	return 0;
}

static int mpeg4_hevc_add(struct h265_annexbtomp4_handle_t* mp4, uint8_t type, const uint8_t* nalu, int bytes)
{
	// copy new
	assert(mp4->hevc->numOfArrays < sizeof(mp4->hevc->nalu) / sizeof(mp4->hevc->nalu[0]));
	if (mp4->hevc->numOfArrays >= sizeof(mp4->hevc->nalu) / sizeof(mp4->hevc->nalu[0])
		|| mp4->hevcptr + bytes > mp4->hevc->data + sizeof(mp4->hevc->data))
	{
		assert(0);
		mp4->errcode = -1;
		return -1;
	}

	if (mp4->update) *mp4->update = 1; // set update flag
	mp4->hevc->nalu[mp4->hevc->numOfArrays].type = type;
	mp4->hevc->nalu[mp4->hevc->numOfArrays].bytes = (uint16_t)bytes;
	mp4->hevc->nalu[mp4->hevc->numOfArrays].array_completeness = 1;
	mp4->hevc->nalu[mp4->hevc->numOfArrays].data = mp4->hevcptr;
	memcpy(mp4->hevc->nalu[mp4->hevc->numOfArrays].data, nalu, bytes);
	mp4->hevcptr += bytes;
	++mp4->hevc->numOfArrays;
	return 0;
}

static int h265_vps_copy(struct h265_annexbtomp4_handle_t* mp4, const uint8_t* nalu, int bytes)
{
	int i;
	uint8_t vpsid;

	if (bytes < 3)
	{
		assert(0);
		mp4->errcode = -1;
		return -1; // invalid length
	}

	vpsid = hevc_vps_id(nalu, bytes, mp4->hevc, mp4->hevcptr);
	for (i = 0; i < mp4->hevc->numOfArrays; i++)
	{
		if (H265_NAL_VPS == mp4->hevc->nalu[i].type && vpsid == hevc_vps_id(mp4->hevc->nalu[i].data, mp4->hevc->nalu[i].bytes, mp4->hevc, mp4->hevcptr))
			return mpeg4_hevc_update(mp4, i, nalu, bytes);
	}

	return mpeg4_hevc_add(mp4, H265_NAL_VPS, nalu, bytes);
}

static int h265_sps_copy(struct h265_annexbtomp4_handle_t* mp4, const uint8_t* nalu, int bytes)
{
	int i;
	uint8_t spsid;
	uint8_t vpsid, vpsid2;

	if (bytes < 13 + 2)
	{
		assert(0);
		mp4->errcode = -1;
		return -1; // invalid length
	}

	spsid = hevc_sps_id(nalu, bytes, mp4->hevc, mp4->hevcptr, &vpsid);
	for (i = 0; i < mp4->hevc->numOfArrays; i++)
	{
		if (H265_NAL_SPS == mp4->hevc->nalu[i].type && spsid == hevc_sps_id(mp4->hevc->nalu[i].data, mp4->hevc->nalu[i].bytes, mp4->hevc, mp4->hevcptr, &vpsid2) && vpsid == vpsid2)
			return mpeg4_hevc_update(mp4, i, nalu, bytes);
	}

	return mpeg4_hevc_add(mp4, H265_NAL_SPS, nalu, bytes);
}

static int h265_pps_copy(struct h265_annexbtomp4_handle_t* mp4, const uint8_t* nalu, int bytes)
{
	int i;
	uint8_t ppsid;
	uint8_t spsid, spsid2;

	if (bytes < 1 + 2)
	{
		assert(0);
		mp4->errcode = -1;
		return -1; // invalid length
	}

	ppsid = hevc_pps_id(nalu, bytes, mp4->hevc, mp4->hevcptr, &spsid);
	for (i = 0; i < mp4->hevc->numOfArrays; i++)
	{
		if (H265_NAL_PPS == mp4->hevc->nalu[i].type && ppsid == hevc_pps_id(mp4->hevc->nalu[i].data, mp4->hevc->nalu[i].bytes, mp4->hevc, mp4->hevcptr, &spsid2) && spsid == spsid2)
			return mpeg4_hevc_update(mp4, i, nalu, bytes);
	}

	return mpeg4_hevc_add(mp4, H265_NAL_PPS, nalu, bytes);
}

static int h265_sei_clear(struct h265_annexbtomp4_handle_t* mp4)
{
	int i;
	struct mpeg4_hevc_t* hevc;
	hevc = mp4->hevc;

	for (i = 0; i < hevc->numOfArrays; i++)
	{
		if (H265_NAL_SEI_PREFIX == hevc->nalu[i].type || H265_NAL_SEI_SUFFIX == hevc->nalu[i].type)
		{
			mpeg4_hevc_remove(hevc, hevc->nalu[i].data, hevc->nalu[i].bytes, mp4->hevcptr);
			mp4->hevcptr -= mp4->hevc->nalu[i].bytes;
			if(i + 1 < hevc->numOfArrays)
				memmove(mp4->hevc->nalu + i, mp4->hevc->nalu + i + 1, sizeof(mp4->hevc->nalu[0]) * (hevc->numOfArrays - i - 1));
			--hevc->numOfArrays;
			--i;
		}
	}
	return 0;
}

static void hevc_handler(void* param, const uint8_t* nalu, int bytes)
{
	uint8_t nalutype;
	struct h265_annexbtomp4_handle_t* mp4;
	mp4 = (struct h265_annexbtomp4_handle_t*)param;

	nalutype = (nalu[0] >> 1) & 0x3f;
	switch (nalutype)
	{
	case H265_NAL_VPS:
		h265_sei_clear(mp4); // remove all prefix/suffix sei
		h265_vps_copy(mp4, nalu, bytes);
		break;

	case H265_NAL_SPS:
		h265_sps_copy(mp4, nalu, bytes);
		break;

	case H265_NAL_PPS:
		h265_pps_copy(mp4, nalu, bytes);
		break;

#if defined(H265_FILTER_SEI)
	case H265_NAL_SEI_PREFIX:
		mpeg4_hevc_add(mp4, H265_NAL_SEI_PREFIX, nalu, bytes);
		break;

	case H265_NAL_SEI_SUFFIX:
		mpeg4_hevc_add(mp4, H265_NAL_SEI_SUFFIX, nalu, bytes);
		break;
#endif

#if defined(H2645_FILTER_AUD)
	case H265_NAL_AUD:
		return; // ignore AUD
#endif
	}

	// IRAP-1, B/P-2, other-0
	if (mp4->vcl && nalutype < H265_NAL_VPS)
		*mp4->vcl = 16<=nalutype && nalutype<=23 ? 1 : 2;

	if (mp4->capacity >= mp4->bytes + bytes + 4)
	{
		mp4->out[mp4->bytes + 0] = (uint8_t)((bytes >> 24) & 0xFF);
		mp4->out[mp4->bytes + 1] = (uint8_t)((bytes >> 16) & 0xFF);
		mp4->out[mp4->bytes + 2] = (uint8_t)((bytes >> 8) & 0xFF);
		mp4->out[mp4->bytes + 3] = (uint8_t)((bytes >> 0) & 0xFF);
		memcpy(mp4->out + mp4->bytes + 4, nalu, bytes);
		mp4->bytes += bytes + 4;
	}
	else
	{
		mp4->errcode = -1;
	}
}

int h265_annexbtomp4(struct mpeg4_hevc_t* hevc, const void* data, int bytes, void* out, int size, int *vcl, int* update)
{
	int i;
	struct h265_annexbtomp4_handle_t h;
	memset(&h, 0, sizeof(h));
	h.hevcptr = hevc->data;
	h.hevc = hevc;
	h.vcl = vcl;
	h.update = update;
	h.out = (uint8_t*)out;
	h.capacity = size;
	if (vcl) *vcl = 0;
	if (update) *update = 0;

	for (i = 0; i < hevc->numOfArrays; i++)
	{
		if (hevc->nalu[i].data >= h.hevcptr)
			h.hevcptr = hevc->nalu[i].data + hevc->nalu[i].bytes;
	}

//	hevc->numTemporalLayers = 0;
//	hevc->temporalIdNested = 0;
//	hevc->min_spatial_segmentation_idc = 0;
//	hevc->general_profile_compatibility_flags = 0xffffffff;
//	hevc->general_constraint_indicator_flags = 0xffffffffffULL;
//	hevc->chromaFormat = 1; // 4:2:0

	mpeg4_h264_annexb_nalu((const uint8_t*)data, bytes, hevc_handler, &h);
	hevc->configurationVersion = 1;
	hevc->lengthSizeMinusOne = 3; // 4 bytes
	return 0 == h.errcode ? h.bytes : 0;
}

int h265_is_new_access_unit(const uint8_t* nalu, size_t bytes)
{
    enum { NAL_VPS = 32, NAL_SPS = 33, NAL_PPS = 34, NAL_AUD = 35, NAL_PREFIX_SEI = 39, };
    
    uint8_t nal_type;
    uint8_t nuh_layer_id;
    
    if(bytes < 3)
        return 0;
    
    nal_type = (nalu[0] >> 1) & 0x3f;
    nuh_layer_id = ((nalu[0] & 0x01) << 5) | ((nalu[1] >> 3) &0x1F);
    
    // 7.4.2.4.4 Order of NAL units and coded pictures and their association to access units
    if(NAL_VPS == nal_type || NAL_SPS == nal_type || NAL_PPS == nal_type ||
       (nuh_layer_id == 0 && (NAL_AUD == nal_type || NAL_PREFIX_SEI == nal_type || (41 <= nal_type && nal_type <= 44) || (48 <= nal_type && nal_type <= 55))))
        return 1;
        
    // 7.4.2.4.5 Order of VCL NAL units and association to coded pictures
    if (nal_type <= 31)
    {
        //first_slice_segment_in_pic_flag 0x80
        return (nalu[2] & 0x80) ? 1 : 0;
    }
    
    return 0;
}

#if defined(_DEBUG) || defined(DEBUG)
void hevc_annexbtomp4_test(void)
{
	const uint8_t vps[] = { 0x40, 0x01, 0x0c, 0x01, 0xff, 0xff, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00, 0x80, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x78, 0x9d, 0xc0, 0x90 };
	const uint8_t sps[] = { 0x42, 0x01, 0x01, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00, 0x80, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x78, 0xa0, 0x03, 0xc0, 0x80, 0x32, 0x16, 0x59, 0xde, 0x49, 0x1b, 0x6b, 0x80, 0x40, 0x00, 0x00, 0xfa, 0x00, 0x00, 0x17, 0x70, 0x02 };
	const uint8_t pps[] = { 0x44, 0x01, 0xc1, 0x73, 0xd1, 0x89 };
	const uint8_t annexb[] = { 0x00, 0x00, 0x00, 0x01, 0x4e, 0x01, 0x06, 0x01, 0xd0, 0x80, 0x00, 0x00, 0x00, 0x01, 0x40, 0x01, 0x0c, 0x01, 0xff, 0xff, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00, 0x80, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x78, 0x9d, 0xc0, 0x90, 0x00, 0x00, 0x00, 0x01, 0x42, 0x01, 0x01, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00, 0x80, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x78, 0xa0, 0x03, 0xc0, 0x80, 0x32, 0x16, 0x59, 0xde, 0x49, 0x1b, 0x6b, 0x80, 0x40, 0x00, 0x00, 0xfa, 0x00, 0x00, 0x17, 0x70, 0x02, 0x00, 0x00, 0x00, 0x01, 0x44, 0x01, 0xc1, 0x73, 0xd1, 0x89 };
	uint8_t output[512];
	int vcl, update;

	struct mpeg4_hevc_t hevc;
	memset(&hevc, 0, sizeof(hevc));
	assert(h265_annexbtomp4(&hevc, annexb, sizeof(annexb), output, sizeof(output), &vcl, &update) > 0);
	assert(3 == hevc.numOfArrays && vcl == 0 && update == 1);
	assert(hevc.nalu[0].bytes == sizeof(vps) && 0 == memcmp(hevc.nalu[0].data, vps, sizeof(vps)));
	assert(hevc.nalu[1].bytes == sizeof(sps) && 0 == memcmp(hevc.nalu[1].data, sps, sizeof(sps)));
	assert(hevc.nalu[2].bytes == sizeof(pps) && 0 == memcmp(hevc.nalu[2].data, pps, sizeof(pps)));
}
#endif
