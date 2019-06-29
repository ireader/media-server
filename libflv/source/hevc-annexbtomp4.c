#include "mpeg4-hevc.h"
#include <string.h>
#include <assert.h>

#define H265_NAL_VPS		32
#define H265_NAL_SPS		33
#define H265_NAL_PPS		34
#define H265_NAL_AUD		35

#define MAX(x, y) ((x) > (y) ? (x) : (y))

struct h265_annexbtomp4_handle_t
{
	struct mpeg4_hevc_t* hevc;
	uint8_t* hevcptr;
	int errcode;
	int* vcl;

	uint8_t* ptr;
	size_t bytes;
	size_t capacity;
};

void h264_annexb_nalu(const void* h264, size_t bytes, void(*handler)(void* param, const void* nalu, size_t bytes), void* param);

static size_t hevc_rbsp_decode(const uint8_t* nalu, size_t bytes, uint8_t* sodb)
{
	size_t i, j;
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

static void hevc_profile_tier_level(const uint8_t* nalu, size_t bytes, uint8_t maxNumSubLayersMinus1, struct mpeg4_hevc_t* hevc)
{
	uint8_t i;
//	uint8_t sub_layer_profile_present_flag[8];
//	uint8_t sub_layer_level_present_flag[8];

	if (bytes < 12)
		return;

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

	// TODO:
	for (i = 0; i < maxNumSubLayersMinus1; i++)
	{
		//sub_layer_profile_present_flag[i];
		//sub_layer_level_present_flag[i];
	}
}

static void mpeg4_hevc_clear(struct h265_annexbtomp4_handle_t* mp4)
{
	if (NULL != mp4->hevcptr)
		return;
	memset(mp4->hevc, 0, sizeof(*mp4->hevc));
	mp4->hevc->general_profile_compatibility_flags = 0xffffffff;
	mp4->hevc->general_constraint_indicator_flags = 0xffffffffffULL;
	mp4->hevc->chromaFormat = 1; // 4:2:0
	mp4->hevc->numOfArrays = 0;
	mp4->hevcptr = mp4->hevc->data;
}

static void hevc_handler(void* param, const uint8_t* nalu, size_t bytes)
{
	uint8_t nalutype;
	size_t sodb_bytes;
	struct h265_annexbtomp4_handle_t* mp4;
	mp4 = (struct h265_annexbtomp4_handle_t*)param;

	nalutype = (nalu[0] >> 1) & 0x3f;
	switch (nalutype)
	{
	case H265_NAL_VPS:
	case H265_NAL_SPS:
	case H265_NAL_PPS:
		mpeg4_hevc_clear(mp4);
		if (mp4->hevc->numOfArrays >= sizeof(mp4->hevc->nalu) / sizeof(mp4->hevc->nalu[0])
			|| mp4->hevcptr + bytes >= mp4->hevc->data + sizeof(mp4->hevc->data))
		{
			assert(0);
			mp4->errcode = -1;
			return;
		}

		sodb_bytes = hevc_rbsp_decode(nalu, bytes, mp4->hevcptr);

		if (nalutype == H265_NAL_VPS)
		{
			uint8_t vps_max_sub_layers_minus1 = (nalu[3] >> 1) & 0x07;
			uint8_t vps_temporal_id_nesting_flag = nalu[3] & 0x01;
			mp4->hevc->numTemporalLayers = MAX(mp4->hevc->numTemporalLayers, vps_max_sub_layers_minus1 + 1);
			mp4->hevc->temporalIdNested = (mp4->hevc->temporalIdNested || vps_temporal_id_nesting_flag) ? 1 : 0;
			hevc_profile_tier_level(mp4->hevcptr + 6, sodb_bytes - 6, vps_max_sub_layers_minus1, mp4->hevc);
		}
		else if (nalutype == H265_NAL_SPS)
		{
			// TODO:
			//mp4->hevc->chromaFormat; // chroma_format_idc
			//mp4->hevc->bitDepthLumaMinus8; // bit_depth_luma_minus8
			//mp4->hevc->bitDepthChromaMinus8; // bit_depth_chroma_minus8

			// TODO: vui_parameters
			//mp4->hevc->min_spatial_segmentation_idc; // min_spatial_segmentation_idc
		}
		else if (nalutype == H265_NAL_PPS)
		{
			// TODO:
			//mp4->hevc->parallelismType; // entropy_coding_sync_enabled_flag
		}

		mp4->hevc->nalu[mp4->hevc->numOfArrays].type = nalutype;
		mp4->hevc->nalu[mp4->hevc->numOfArrays].bytes = (uint16_t)bytes;
		mp4->hevc->nalu[mp4->hevc->numOfArrays].array_completeness = 1;
		mp4->hevc->nalu[mp4->hevc->numOfArrays].data = mp4->hevcptr;
		memcpy(mp4->hevc->nalu[mp4->hevc->numOfArrays].data, nalu, bytes);
		mp4->hevcptr += bytes;
		++mp4->hevc->numOfArrays;
		return;

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

int h265_annexbtomp4(struct mpeg4_hevc_t* hevc, const void* data, int bytes, void* out, int size, int *vcl)
{
	struct h265_annexbtomp4_handle_t h;
	memset(&h, 0, sizeof(h));
	h.hevc = hevc;
	h.vcl = vcl;
	h.ptr = (uint8_t*)out;
	h.capacity = size;
	if (vcl) *vcl = 0;

	hevc->numTemporalLayers = 0;
	hevc->temporalIdNested = 0;
	hevc->min_spatial_segmentation_idc = 0;
	
	h264_annexb_nalu((const uint8_t*)data, bytes, hevc_handler, &h);
	hevc->configurationVersion = 1;
	hevc->lengthSizeMinusOne = 3; // 4 bytes
	return 0 == h.errcode ? h.bytes : 0;
}
