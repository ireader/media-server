#include "mpeg4-hevc.h"
#include <string.h>
#include <assert.h>

#define H265_VPS		32
#define H265_SPS		33
#define H265_PPS		34

#define MAX(x, y) ((x) > (y) ? (x) : (y))

struct hevc_handle_t
{
	struct mpeg4_hevc_t* hevc;
	int errcode;

	uint8_t* ptr;
	size_t bytes;
	size_t capacity;
};

typedef void(*hevc_nalu_handler)(void* param, const uint8_t* nalu, size_t bytes);

static const uint8_t* hevc_startcode(const uint8_t *data, size_t bytes)
{
	size_t i;
	for (i = 2; i + 1 < bytes; i++)
	{
		if (0x01 == data[i] && 0x00 == data[i - 1] && 0x00 == data[i - 2])
			return data + i + 1;
	}

	return NULL;
}

///@param[in] hevc H.265 byte stream format data(A set of NAL units)
static void hevc_stream(const uint8_t* hevc, size_t bytes, hevc_nalu_handler handler, void* param)
{
	ptrdiff_t n;
	const uint8_t* p, *next, *end;

	end = hevc + bytes;
	p = hevc_startcode(hevc, bytes);

	while (p)
	{
		next = hevc_startcode(p, end - p);
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
	uint8_t sub_layer_profile_present_flag[8];
	uint8_t sub_layer_level_present_flag[8];

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
	hevc->general_constraint_indicator_flags |= nalu[7] << 24;
	hevc->general_constraint_indicator_flags |= nalu[8] << 8;
	hevc->general_constraint_indicator_flags |= nalu[9] << 8;
	hevc->general_constraint_indicator_flags |= nalu[10];

	hevc->general_level_idc = nalu[11];

	// TODO:
	for (i = 0; i < maxNumSubLayersMinus1; i++)
	{
		sub_layer_profile_present_flag[i];
		sub_layer_level_present_flag[i];
	}
}

static void hevc_handler(void* param, const uint8_t* nalu, size_t bytes)
{
	uint8_t* sodb;
	uint8_t nal_type;
	size_t sodb_bytes;
	struct hevc_handle_t* mp4;
	mp4 = (struct hevc_handle_t*)param;

	nal_type = (nalu[0] >> 1) & 0x3f;
	switch (nal_type)
	{
	case H265_VPS:
	case H265_SPS:
	case H265_PPS:
		sodb = mp4->hevc->numOfArrays > 0 ? mp4->hevc->nalu[mp4->hevc->numOfArrays - 1].data + mp4->hevc->nalu[mp4->hevc->numOfArrays - 1].bytes : mp4->hevc->data;
		if (mp4->hevc->numOfArrays >= sizeof(mp4->hevc->nalu) / sizeof(mp4->hevc->nalu[0])
			|| sodb + bytes >= mp4->hevc->data + sizeof(mp4->hevc->data))
		{
			mp4->errcode = -1;
			return;
		}

		sodb_bytes = hevc_rbsp_decode(nalu, bytes, sodb);

		if (nal_type == H265_VPS)
		{
			uint8_t vps_max_sub_layers_minus1 = (nalu[3] >> 1) & 0x07;
			uint8_t vps_temporal_id_nesting_flag = nalu[3] & 0x01;
			mp4->hevc->numTemporalLayers = MAX(mp4->hevc->numTemporalLayers, vps_max_sub_layers_minus1 + 1);
			mp4->hevc->temporalIdNested = (mp4->hevc->temporalIdNested || vps_temporal_id_nesting_flag) ? 1 : 0;
			hevc_profile_tier_level(sodb + 6, sodb_bytes - 6, vps_max_sub_layers_minus1, mp4->hevc);
		}
		else if (nal_type == H265_SPS)
		{
			// TODO:
			mp4->hevc->chromaFormat; // chroma_format_idc
			mp4->hevc->bitDepthLumaMinus8; // bit_depth_luma_minus8
			mp4->hevc->bitDepthChromaMinus8; // bit_depth_chroma_minus8

			// TODO: vui_parameters
			mp4->hevc->min_spatial_segmentation_idc; // min_spatial_segmentation_idc
		}
		else if (nal_type == H265_PPS)
		{
			// TODO:
			mp4->hevc->parallelismType; // entropy_coding_sync_enabled_flag
		}

		mp4->hevc->nalu[mp4->hevc->numOfArrays].type = nal_type;
		mp4->hevc->nalu[mp4->hevc->numOfArrays].bytes = (uint16_t)bytes;
		mp4->hevc->nalu[mp4->hevc->numOfArrays].array_completeness = 1;
		mp4->hevc->nalu[mp4->hevc->numOfArrays].data = sodb;
		memcpy(mp4->hevc->nalu[mp4->hevc->numOfArrays].data, nalu, bytes);
		++mp4->hevc->numOfArrays;
		return;

	case 16: // BLA_W_LP
	case 17: // BLA_W_RADL
	case 18: // BLA_N_LP
	case 19: // IDR_W_RADL
	case 20: // IDR_N_LP
	case 21: // CRA_NUT
	case 22: // RSV_IRAP_VCL22
	case 23: // RSV_IRAP_VCL23
		mp4->hevc->constantFrameRate = 0x04; // irap frame
		break;

	default:
		break;
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

size_t hevc_annexbtomp4(struct mpeg4_hevc_t* hevc, const void* data, size_t bytes, void* out, size_t size)
{
	struct hevc_handle_t h;
	h.hevc = hevc;
	h.ptr = (uint8_t*)out;
	h.capacity = size;
	h.bytes = 0;
	h.errcode = 0;

	hevc->configurationVersion = 1;
	hevc->lengthSizeMinusOne = 3; // 4 bytes
	hevc->numTemporalLayers = 0;
	hevc->temporalIdNested = 0;
	hevc->general_profile_compatibility_flags = 0xffffffff;
	hevc->general_constraint_indicator_flags = 0xffffffffffULL;
	hevc->min_spatial_segmentation_idc = 0;
	hevc->chromaFormat = 1; // 4:2:0
	hevc->bitDepthLumaMinus8 = 0;
	hevc->bitDepthChromaMinus8 = 0;
	hevc->avgFrameRate = 0;
	hevc->constantFrameRate = 0;
	hevc->numOfArrays = 0;

	hevc_stream((const uint8_t*)data, bytes, hevc_handler, &h);
	return 0 == h.errcode ? h.bytes : 0;
}
