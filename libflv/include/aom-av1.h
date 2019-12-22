#ifndef _aom_av1_h_
#define _aom_av1_h_

#include <stdint.h>
#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

struct aom_av1_t
{
	uint32_t marker : 1;
	uint32_t version : 7;
	uint32_t seq_profile : 3;
	uint32_t seq_level_idx_0 : 5;
	uint32_t seq_tier_0 : 1;
	uint32_t high_bitdepth : 1;
	uint32_t twelve_bit : 1;
	uint32_t monochrome : 1;
	uint32_t chroma_subsampling_x : 1;
	uint32_t chroma_subsampling_y : 1;
	uint32_t chroma_sample_position : 2;

	uint32_t reserved : 3;
	uint32_t initial_presentation_delay_present : 1;
	uint32_t initial_presentation_delay_minus_one : 4;

	uint16_t bytes;
	uint8_t data[2 * 1024];
};

int aom_av1_codec_configuration_record_load(const uint8_t* data, size_t bytes, struct aom_av1_t* av1);
int aom_av1_codec_configuration_record_save(const struct aom_av1_t* av1, uint8_t* data, size_t bytes);

#if defined(__cplusplus)
}
#endif
#endif /* !_aom_av1_h_ */
