#include "aom-av1.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// https://aomediacodec.github.io/av1-isobmff
// https://aomediacodec.github.io/av1-avif/

/*
aligned (8) class AV1CodecConfigurationRecord {
  unsigned int (1) marker = 1;
  unsigned int (7) version = 1;
  unsigned int (3) seq_profile;
  unsigned int (5) seq_level_idx_0;
  unsigned int (1) seq_tier_0;
  unsigned int (1) high_bitdepth;
  unsigned int (1) twelve_bit;
  unsigned int (1) monochrome;
  unsigned int (1) chroma_subsampling_x;
  unsigned int (1) chroma_subsampling_y;
  unsigned int (2) chroma_sample_position;
  unsigned int (3) reserved = 0;

  unsigned int (1) initial_presentation_delay_present;
  if (initial_presentation_delay_present) {
	unsigned int (4) initial_presentation_delay_minus_one;
  } else {
	unsigned int (4) reserved = 0;
  }

  unsigned int (8)[] configOBUs;
}
*/

int aom_av1_codec_configuration_record_load(const uint8_t* data, size_t bytes, struct aom_av1_t* av1)
{
	if (bytes < 4)
		return -1;

	av1->marker = data[0] >> 7;
	av1->version = data[0] & 0x7F;
	av1->seq_profile = data[1] >> 5;
	av1->seq_level_idx_0 = data[1] & 0x1F;

	av1->seq_tier_0 = data[2] >> 7;
	av1->high_bitdepth = (data[2] >> 6) & 0x01;
	av1->twelve_bit = (data[2] >> 5) & 0x01;
	av1->monochrome = (data[2] >> 4) & 0x01;
	av1->chroma_subsampling_x = (data[2] >> 3) & 0x01;
	av1->chroma_subsampling_y = (data[2] >> 2) & 0x01;
	av1->chroma_sample_position = data[2] & 0x03;

	av1->reserved = data[3] >> 5;
	av1->initial_presentation_delay_present = (data[3] >> 4) & 0x01;
	av1->initial_presentation_delay_minus_one = data[3] & 0x0F;

	if (bytes - 4 > sizeof(av1->data))
		return -1;

	av1->bytes = (uint16_t)(bytes - 4);
	memcpy(av1->data, data + 4, av1->bytes);
	return (int)bytes;
}

int aom_av1_codec_configuration_record_save(const struct aom_av1_t* av1, uint8_t* data, size_t bytes)
{
	if (bytes < (size_t)av1->bytes + 4)
		return 0; // don't have enough memory

	data[0] = (uint8_t)((av1->marker << 7) | av1->version);
	data[1] = (uint8_t)((av1->seq_profile << 5) | av1->seq_level_idx_0);
	data[2] = (uint8_t)((av1->seq_tier_0 << 7) | (av1->high_bitdepth << 6) | (av1->twelve_bit << 5) | (av1->monochrome << 4) | (av1->chroma_subsampling_x << 3) | (av1->chroma_subsampling_y << 2) | av1->chroma_sample_position);
	data[3] = (uint8_t)((av1->initial_presentation_delay_present << 4) | av1->initial_presentation_delay_minus_one);

	memcpy(data + 4, av1->data, av1->bytes);
	return av1->bytes + 4;
}

static inline const uint8_t* leb128(const uint8_t* data, size_t bytes, int64_t* v)
{
	size_t i;
	int64_t b;

	b = 0x80;
	for (*v = i = 0; i < 8 && i < bytes && 0 != (b & 0x80); i++)
	{
		b = data[i];
		*v |= (b & 0x7F) << (i * 7);
	}
	return data + i;
}

#if defined(_DEBUG) || defined(DEBUG)
void aom_av1_test(void)
{
	const unsigned char src[] = {
		0x81, 0x04, 0x0c, 0x00, 0x0a, 0x0b, 0x00, 0x00, 0x00, 0x24, 0xcf, 0x7f, 0x0d, 0xbf, 0xff, 0x30, 0x08
	};
	unsigned char data[sizeof(src)];

	struct aom_av1_t av1;
	assert(sizeof(src) == aom_av1_codec_configuration_record_load(src, sizeof(src), &av1));
	assert(1 == av1.version && 0 == av1.seq_profile && 4 == av1.seq_level_idx_0);
	assert(0 == av1.seq_tier_0 && 0 == av1.high_bitdepth && 0 == av1.twelve_bit && 0 == av1.monochrome && 1 == av1.chroma_subsampling_x && 1 == av1.chroma_subsampling_y && 0 == av1.chroma_sample_position);
	assert(0 == av1.initial_presentation_delay_present && 0 == av1.initial_presentation_delay_minus_one);
	assert(13 == av1.bytes);
	assert(sizeof(src) == aom_av1_codec_configuration_record_save(&av1, data, sizeof(data)));
	assert(0 == memcmp(src, data, sizeof(src)));
}
#endif
