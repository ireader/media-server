#include "aom-av1.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include "mpeg4-bits.h"

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

static int aom_av1_low_overhead_bitstream_obu(const uint8_t* data, size_t bytes, int (*handler)(void* param, const uint8_t* obu, size_t bytes), void* param)
{
	int r;
	int64_t n[3];
	const uint8_t* temporal, * frame, * obu;

	r = 0;
	for (temporal = data; temporal < data + bytes && 0 == r; temporal += n[0])
	{
		// temporal_unit_size
		temporal = leb128(temporal, data + bytes - temporal, &n[0]);
		for (frame = temporal; frame < temporal + n[0] && 0 == r; frame += n[1])
		{
			// frame_unit_size
			frame = leb128(frame, temporal + n[0] - frame, &n[1]);
			for (obu = frame; obu < frame + n[1] && 0 == r; obu += n[2])
			{
				obu = leb128(obu, frame + n[1] - obu, &n[2]);
				r = handler(param, obu, (size_t)n[2]);
			}
		}
	}

	return r;
}

// http://aomedia.org/av1/specification/syntax/#color-config-syntax
static int aom_av1_color_config(struct mpeg4_bits_t* bits, struct aom_av1_t* av1)
{
	uint8_t BitDepth;
	uint8_t color_primaries;
	uint8_t transfer_characteristics;
	uint8_t matrix_coefficients;

	av1->high_bitdepth = mpeg4_bits_read(bits);
	if (av1->seq_profile == 2 && av1->high_bitdepth)
	{
		av1->twelve_bit = mpeg4_bits_read(bits);
		BitDepth = av1->twelve_bit ? 12 : 10;
	}
	else if (av1->seq_profile <= 2)
	{
		BitDepth = av1->high_bitdepth ? 10 : 8;
	}
	else
	{
		assert(0);
		BitDepth = 8;
	}

	if (av1->seq_profile == 1)
	{
		av1->monochrome = 0;
	}
	else
	{
		av1->monochrome = mpeg4_bits_read(bits);
	}

	if (mpeg4_bits_read(bits)) // color_description_present_flag
	{
		color_primaries = mpeg4_bits_read_uint8(bits, 8); // color_primaries
		transfer_characteristics = mpeg4_bits_read_uint8(bits, 8); // transfer_characteristics
		matrix_coefficients = mpeg4_bits_read_uint8(bits, 8); // matrix_coefficients
	}
	else
	{
		// http://aomedia.org/av1/specification/semantics/#color-config-semantics
		color_primaries = 2; // CP_UNSPECIFIED;
		transfer_characteristics = 2; // TC_UNSPECIFIED;
		matrix_coefficients = 2; // MC_UNSPECIFIED;
	}

	if (av1->monochrome)
	{
		mpeg4_bits_read(bits); // color_range
		av1->chroma_subsampling_x = 1;
		av1->chroma_subsampling_y = 1;
	}
	else if (color_primaries == 1 /*CP_BT_709*/ && transfer_characteristics == 13 /*TC_SRGB*/ && matrix_coefficients == 0 /*MC_IDENTITY*/)
	{
		av1->chroma_subsampling_x = 0;
		av1->chroma_subsampling_y = 0;
	}
	else
	{
		mpeg4_bits_read(bits); // color_range
		if (av1->seq_profile == 0)
		{
			av1->chroma_subsampling_x = 1;
			av1->chroma_subsampling_y = 1;
		}
		else if (av1->seq_profile == 1)
		{
			av1->chroma_subsampling_x = 0;
			av1->chroma_subsampling_y = 0;
		}
		else
		{
			if (BitDepth == 12)
			{
				av1->chroma_subsampling_x = mpeg4_bits_read(bits);
				if (av1->chroma_subsampling_x)
					av1->chroma_subsampling_y = mpeg4_bits_read(bits);
				else
					av1->chroma_subsampling_y = 0;
			}
			else
			{
				av1->chroma_subsampling_x = 1;
				av1->chroma_subsampling_y = 0;
			}
		}

		if (av1->chroma_subsampling_x && av1->chroma_subsampling_y)
			av1->chroma_sample_position = mpeg4_bits_read_uint32(bits, 2);
	}

	mpeg4_bits_read(bits); // separate_uv_delta_q
	return 0;
}

// http://aomedia.org/av1/specification/syntax/#timing-info-syntax
static int aom_av1_timing_info(struct mpeg4_bits_t* bits, struct aom_av1_t* av1)
{
	(void)av1;
	mpeg4_bits_read_n(bits, 32); // num_units_in_display_tick
	mpeg4_bits_read_n(bits, 32); // time_scale
	if(mpeg4_bits_read(bits)) // equal_picture_interval
		mpeg4_bits_read_uvlc(bits); // num_ticks_per_picture_minus_1
	return 0;
}

// http://aomedia.org/av1/specification/syntax/#decoder-model-info-syntax
static int aom_av1_decoder_model_info(struct mpeg4_bits_t* bits, struct aom_av1_t* av1)
{
	av1->buffer_delay_length_minus_1 = mpeg4_bits_read_uint8(bits, 5); // buffer_delay_length_minus_1
	mpeg4_bits_read_n(bits, 32); // num_units_in_decoding_tick
	mpeg4_bits_read_n(bits, 5); // buffer_removal_time_length_minus_1
	mpeg4_bits_read_n(bits, 5); // frame_presentation_time_length_minus_1
	return 0;
}

// http://aomedia.org/av1/specification/syntax/#operating-parameters-info-syntax
static int aom_av1_operating_parameters_info(struct mpeg4_bits_t* bits, struct aom_av1_t* av1, int op)
{
	uint8_t n;
	n = av1->buffer_delay_length_minus_1 + 1;
	mpeg4_bits_read_n(bits, n); // decoder_buffer_delay[ op ]
	mpeg4_bits_read_n(bits, n); // encoder_buffer_delay[ op ]
	mpeg4_bits_read(bits); // low_delay_mode_flag[ op ]
	(void)op;
	return 0;
}

// http://aomedia.org/av1/specification/syntax/#sequence-header-obu-syntax
static int aom_av1_sequence_header_obu(struct aom_av1_t* av1, const void* data, size_t bytes)
{
	uint8_t i;
	uint8_t reduced_still_picture_header;
	uint8_t decoder_model_info_present_flag;
	uint8_t operating_points_cnt_minus_1;
	uint8_t frame_width_bits_minus_1;
	uint8_t frame_height_bits_minus_1;
	uint8_t enable_order_hint;
	uint8_t seq_force_screen_content_tools;
	struct mpeg4_bits_t bits;

	mpeg4_bits_init(&bits, (void*)data, bytes);
	av1->seq_profile = mpeg4_bits_read_uint32(&bits, 3);
	mpeg4_bits_read(&bits); // still_picture
	reduced_still_picture_header = mpeg4_bits_read_uint8(&bits, 1);
	if (reduced_still_picture_header)
	{
		av1->initial_presentation_delay_present = 0; // initial_display_delay_present_flag
		av1->seq_level_idx_0 = mpeg4_bits_read_uint32(&bits, 5);
		av1->seq_tier_0 = 0;
		decoder_model_info_present_flag = 0;
	}
	else
	{
		if (mpeg4_bits_read(&bits)) // timing_info_present_flag
		{
			// timing_info( )
			aom_av1_timing_info(&bits, av1);

			decoder_model_info_present_flag = mpeg4_bits_read_uint8(&bits, 1); // decoder_model_info_present_flag
			if (decoder_model_info_present_flag)
			{
				// decoder_model_info( )
				aom_av1_decoder_model_info(&bits, av1);
			}
		}
		else
		{
			decoder_model_info_present_flag = 0;
		}

		av1->initial_presentation_delay_present = mpeg4_bits_read(&bits); //  initial_display_delay_present_flag =
		operating_points_cnt_minus_1 = mpeg4_bits_read_uint8(&bits, 5);
		for (i = 0; i < operating_points_cnt_minus_1; i++)
		{
			uint8_t seq_level_idx;
			uint8_t seq_tier;
			uint8_t initial_display_delay_minus_1;

			mpeg4_bits_read_n(&bits, 12); // operating_point_idc[ i ]
			seq_level_idx = mpeg4_bits_read_uint8(&bits, 5); // seq_level_idx[ i ]
			if (seq_level_idx > 7)
			{
				seq_tier = mpeg4_bits_read_uint8(&bits, 1); // seq_tier[ i ]
			}
			else
			{
				seq_tier = 0;
			}

			if (decoder_model_info_present_flag)
			{
				if (mpeg4_bits_read(&bits)) // decoder_model_present_for_this_op[i]
				{
					aom_av1_operating_parameters_info(&bits, av1, i);
				}
			}

			if (av1->initial_presentation_delay_present && mpeg4_bits_read(&bits)) // initial_display_delay_present_for_this_op[ i ]
				initial_display_delay_minus_1 = mpeg4_bits_read_uint8(&bits, 4); // initial_display_delay_minus_1[ i ]
			else
				initial_display_delay_minus_1 = 0;

			if (0 == i)
			{
				av1->seq_level_idx_0 = seq_level_idx;
				av1->seq_tier_0 = seq_tier;
				av1->initial_presentation_delay_minus_one = initial_display_delay_minus_1;
			}
		}
	}

	// choose_operating_point( )
	frame_width_bits_minus_1 = mpeg4_bits_read_uint8(&bits, 4);
	frame_height_bits_minus_1 = mpeg4_bits_read_uint8(&bits, 4);
	av1->width = 1 + mpeg4_bits_read_uint32(&bits, frame_width_bits_minus_1 + 1); // max_frame_width_minus_1
	av1->height = 1 + mpeg4_bits_read_uint32(&bits, frame_height_bits_minus_1 + 1); // max_frame_height_minus_1

	if (reduced_still_picture_header && mpeg4_bits_read(&bits)) // frame_id_numbers_present_flag
	{
		mpeg4_bits_read_n(&bits, 4); // delta_frame_id_length_minus_2
		mpeg4_bits_read_n(&bits, 3); // additional_frame_id_length_minus_1
	}

	mpeg4_bits_read(&bits); // use_128x128_superblock
	mpeg4_bits_read(&bits); // enable_filter_intra
	mpeg4_bits_read(&bits); // enable_intra_edge_filter

	if (!reduced_still_picture_header)
	{
		mpeg4_bits_read(&bits); // enable_interintra_compound
		mpeg4_bits_read(&bits); // enable_masked_compound
		mpeg4_bits_read(&bits); // enable_warped_motion
		mpeg4_bits_read(&bits); // enable_dual_filter
		enable_order_hint = mpeg4_bits_read_uint8(&bits, 1);
		if (enable_order_hint)
		{
			mpeg4_bits_read(&bits); // enable_jnt_comp
			mpeg4_bits_read(&bits); // enable_ref_frame_mvs
		}
		if (mpeg4_bits_read(&bits)) // seq_choose_screen_content_tools
		{
			seq_force_screen_content_tools = 2; // SELECT_SCREEN_CONTENT_TOOLS;
		}
		else
		{
			seq_force_screen_content_tools = mpeg4_bits_read_uint8(&bits, 1); // seq_force_screen_content_tools
		}

		if (seq_force_screen_content_tools > 0)
		{
			if (!mpeg4_bits_read(&bits)) // seq_choose_integer_mv
				mpeg4_bits_read(&bits); // seq_force_integer_mv
			//else
			// seq_force_integer_mv = SELECT_INTEGER_MV
		}
		else
		{
			//seq_force_integer_mv = SELECT_INTEGER_MV;
		}

		if (enable_order_hint)
		{
			mpeg4_bits_read(&bits); // order_hint_bits_minus_1
		}
	}

	mpeg4_bits_read(&bits); // enable_superres
	mpeg4_bits_read(&bits); // enable_cdef
	mpeg4_bits_read(&bits); // enable_restoration

	// color_config( )
	aom_av1_color_config(&bits, av1);

	mpeg4_bits_read(&bits); // film_grain_params_present

	return mpeg4_bits_error(&bits) ? -1 : 0;
}

// http://aomedia.org/av1/specification/syntax/#general-obu-syntax
static int aom_av1_obu_handler(void* param, const uint8_t* obu, size_t bytes)
{
	int64_t len;
	size_t offset;
	uint8_t obu_type;
	const uint8_t* ptr;
	struct aom_av1_t* av1;
	
	av1 = (struct aom_av1_t*)param;
	if (bytes < 2)
		return -1;

	// http://aomedia.org/av1/specification/syntax/#obu-header-syntax
	obu_type = (obu[0] >> 3) & 0x0F;
	if ((obu[0] >> 3) & 0x04) // obu_extension_flag
	{
		// http://aomedia.org/av1/specification/syntax/#obu-extension-header-syntax
		// temporal_id = (obu[1] >> 5) & 0x07;
		// spatial_id = (obu[1] >> 3) & 0x03;
		offset = 2;
	}
	else
	{
		offset = 1;
	}

	if (obu[0] & 0x02) // obu_has_size_field
	{
		ptr = leb128(obu + offset, bytes - offset, &len);
		if (ptr + len > obu + bytes)
			return -1;
	}
	else
	{
		ptr = obu + offset;
		len = bytes - offset;
	}

	// http://aomedia.org/av1/specification/semantics/#obu-header-semantics
	if (obu_type == 1 /*OBU_SEQUENCE_HEADER*/ )
	{
		return aom_av1_sequence_header_obu(av1, ptr, (size_t)len);
	}

	return 0;
}

// https://aomediacodec.github.io/av1-isobmff/#av1codecconfigurationbox-section
int aom_av1_codec_configuration_record_init(struct aom_av1_t* av1, const void* data, size_t bytes)
{
	return aom_av1_low_overhead_bitstream_obu((const uint8_t*)data, bytes, aom_av1_obu_handler, av1);
}

int aom_av1_codecs(const struct aom_av1_t* av1, char* codecs, size_t bytes)
{
	unsigned int bitdepth;

	// AV1 5.5.2.Color config syntax
	if (2 == av1->seq_profile && av1->high_bitdepth)
		bitdepth = av1->twelve_bit ? 12 : 10;
	else
		bitdepth = av1->high_bitdepth ? 10 : 8;

	// https://aomediacodec.github.io/av1-isobmff/#codecsparam
	// https://developer.mozilla.org/en-US/docs/Web/Media/Formats/codecs_parameter
	// <sample entry 4CC>.<profile>.<level><tier>.<bitDepth>.<monochrome>.<chromaSubsampling>.<colorPrimaries>.<transferCharacteristics>.<matrixCoefficients>.<videoFullRangeFlag>
	return snprintf(codecs, bytes, "av01.%u.%02u%c.%02u", (unsigned int)av1->seq_profile, (unsigned int)av1->seq_level_idx_0, av1->seq_tier_0 ? 'H' : 'M', (unsigned int)bitdepth);
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

	aom_av1_codecs(&av1, (char*)data, sizeof(data));
	assert(0 == memcmp("av01.0.04M.08", data, 13));
}
#endif
