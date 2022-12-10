#ifndef _mpeg_vvc_h
#define _mpeg_vvc_h

#include <stdint.h>
#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

struct mpeg4_vvc_t
{
	uint32_t lengthSizeMinusOne : 2;	// 2bit,[0,3]
	uint32_t ptl_present_flag : 1;

	// valid on ptl_present_flag
	uint32_t ols_idx : 9;
	uint32_t num_sublayers : 3;
	uint32_t constant_frame_rate : 2;
	uint32_t chroma_format_idc : 2;
	uint32_t bit_depth_minus8 : 2;
	uint16_t max_picture_width;
	uint16_t max_picture_height;
	uint16_t avg_frame_rate;
	struct
	{
		uint32_t num_bytes_constraint_info : 6;
		uint32_t general_profile_idc : 7;
		uint32_t general_tier_flag : 1;
		uint32_t general_level_idc : 8;
		uint32_t ptl_frame_only_constraint_flag : 1;
		uint32_t ptl_multi_layer_enabled_flag : 1;
		uint32_t ptl_sublayer_level_present_flag : 8;
		uint8_t general_constraint_info[64];
		uint8_t sublayer_level_idc[8 - 2];
		uint8_t ptl_num_sub_profiles;
		uint32_t *general_sub_profile_idc; // --> data
	} native_ptl;

	uint8_t  numOfArrays;
	struct
	{
		uint8_t array_completeness;
		uint8_t type; // nalu type
		uint16_t bytes;
		uint8_t* data;
	} nalu[64];

	uint8_t array_completeness;
	uint8_t data[4 * 1024];
	size_t off;
};

int mpeg4_vvc_decoder_configuration_record_load(const uint8_t* data, size_t bytes, struct mpeg4_vvc_t* vvc);

int mpeg4_vvc_decoder_configuration_record_save(const struct mpeg4_vvc_t* vvc, uint8_t* data, size_t bytes);

int mpeg4_vvc_to_nalu(const struct mpeg4_vvc_t* vvc, uint8_t* data, size_t bytes);

int mpeg4_vvc_codecs(const struct mpeg4_vvc_t* vvc, char* codecs, size_t bytes);

int h266_annexbtomp4(struct mpeg4_vvc_t* vvc, const void* data, size_t bytes, void* out, size_t size, int* vcl, int* update);

int h266_mp4toannexb(const struct mpeg4_vvc_t* vvc, const void* data, size_t bytes, void* out, size_t size);

/// h266_is_new_access_unit H.266 new access unit(frame)
/// @return 1-new access, 0-not a new access
int h266_is_new_access_unit(const uint8_t* nalu, size_t bytes);

#if defined(__cplusplus)
}
#endif
#endif /* !_mpeg_vvc_h */
