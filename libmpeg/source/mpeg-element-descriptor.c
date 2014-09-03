#include "mpeg-ps-proto.h"
#include "mpeg-element-descriptor.h"
#include <memory.h>
#include <assert.h>

int video_stream_descriptor(const uint8_t* data, int bytes)
{
	int i;
	video_stream_descriptor_t desc;
	uint8_t descriptor_tag = data[0];
	uint8_t descriptor_len = data[1];

	i = 2;
	memset(&desc, 0, sizeof(desc));
	desc.multiple_frame_rate_flag = (data[i] >> 7) & 0x01;
	desc.frame_rate_code = (data[i] >> 3) & 0x0F;
	desc.MPEG_1_only_flag = (data[i] >> 2) & 0x01;
	desc.constrained_parameter_flag = (data[i] >> 1) & 0x01;
	desc.still_picture_flag = data[i] & 0x01;

	if(0 == desc.MPEG_1_only_flag)
	{
		desc.profile_and_level_indication = data[i+1];
		desc.chroma_format = (data[i+2] >> 6) & 0x03;
		desc.frame_rate_code = (data[i+2] >> 5) & 0x01;
		assert((0x1F & data[i+2]) == 0x00); // 'xxx00000'
	}
}

int audio_stream_descriptor(const uint8_t* data, int bytes)
{
	int i;
	audio_stream_descriptor_t desc;
	uint8_t descriptor_tag = data[0];
	uint8_t descriptor_len = data[1];

	i = 2;
	memset(&desc, 0, sizeof(desc));
	desc.free_format_flag = (data[i] >> 7) & 0x01;
	desc.ID = (data[i] >> 6) & 0x01;
	desc.layer = (data[i] >> 4) & 0x03;
	desc.variable_rate_audio_indicator = (data[i] >> 3) & 0x01;
}

int hierarchy_descriptor(const uint8_t* data, int bytes)
{
	int i;
	hierarchy_descriptor_t desc;
	uint8_t descriptor_tag = data[0];
	uint8_t descriptor_len = data[1];

	i = 2;
	memset(&desc, 0, sizeof(desc));
	desc.temporal_scalability_flag = (data[i] >> 6) & 0x01;
	desc.spatial_scalability_flag = (data[i] >> 5) & 0x01;
	desc.quality_scalability_flag = (data[i] >> 4) & 0x01;
	desc.hierarchy_type = data[i] & 0x0F;
	desc.hierarchy_layer_index = data[i+1] & 0x3F;
	desc.tref_present_flag = (data[i+2] >> 7) & 0x01;
	desc.hierarchy_embedded_layer_index = data[i+2] & 0x3F;
	desc.hierarchy_channel = data[i+3] & 0x3F;
}

int language_descriptor(const uint8_t* data, int bytes)
{
	int i;
	uint8_t descriptor_tag = data[0];
	uint8_t descriptor_len = data[1];

	for(i = 2; i < descriptor_len; i += 4)
	{
		language_descriptor_t desc;
		memset(&desc, 0, sizeof(desc));

		desc.code = (data[i] << 16) | (data[i+1] << 8) | data[i+2];
		desc.audio = data[i+3];
	}
}

int system_clock_descriptor(const uint8_t* data, int bytes)
{
	int i;
	system_clock_descriptor_t desc;
	uint8_t descriptor_tag = data[0];
	uint8_t descriptor_len = data[1];

	i = 2;
	memset(&desc, 0, sizeof(desc));
	desc.external_clock_reference_indicator = (data[i] >> 7) & 0x01;
	desc.clock_accuracy_integer = data[i] & 0x3F;
	desc.clock_accuracy_exponent = (data[i+1] >> 5) & 0x07;
}

int mpeg4_video_descriptor(const uint8_t* data, int bytes)
{
	int i;
	mpeg4_video_descriptor_t desc;
	uint8_t descriptor_tag = data[0];
	uint8_t descriptor_len = data[1];

	i = 2;
	memset(&desc, 0, sizeof(desc));
	desc.visual_profile_and_level = data[i];
}

int mpeg4_audio_descriptor(const uint8_t* data, int bytes)
{
	int i;
	mpeg4_audio_descriptor_t desc;
	uint8_t descriptor_tag = data[0];
	uint8_t descriptor_len = data[1];

	i = 2;
	memset(&desc, 0, sizeof(desc));
	desc.profile_and_level = data[i];
}

int avc_video_descriptor(const uint8_t* data, int bytes)
{
	int i;
	avc_video_descriptor_t desc;
	uint8_t descriptor_tag = data[0];
	uint8_t descriptor_len = data[1];

	i = 2;
	memset(&desc, 0, sizeof(desc));
	desc.profile_idc = data[i];
	desc.constraint_set0_flag = (data[i+1] >> 7) & 0x01;
	desc.constraint_set1_flag = (data[i+1] >> 6) & 0x01;
	desc.constraint_set2_flag = (data[i+1] >> 5) & 0x01;
	desc.constraint_set3_flag = (data[i+1] >> 4) & 0x01;
	desc.constraint_set4_flag = (data[i+1] >> 3) & 0x01;
	desc.constraint_set5_flag = (data[i+1] >> 2) & 0x01;
	desc.AVC_compatible_flags = data[i+1] & 0x03;
	desc.level_idc = data[i+2];
	desc.AVC_still_present = (data[i+3] >> 7) & 0x01;
	desc.AVC_24_hour_picture_flag = (data[i+3] >> 6) & 0x01;
	desc.frame_packing_SEI_not_present_flag = (data[i+3] >> 5) & 0x01;
}

int avc_timing_hrd_descriptor(const uint8_t* data, int bytes)
{
	int i;
	avc_timing_hrd_descriptor_t desc;
	uint8_t descriptor_tag = data[0];
	uint8_t descriptor_len = data[1];

	i = 2;
	memset(&desc, 0, sizeof(desc));
	desc.hrd_management_valid_flag = (data[i] >> 7) & 0x01;
	desc.picture_and_timing_info_present = (data[i] >> 0) & 0x01;
	if(desc.picture_and_timing_info_present)
	{
		desc._90kHZ_flag = (data[i+1] >> 7) & 0x01;
		if(0 == desc._90kHZ_flag)
		{
			desc.N = (data[i+2] << 24) | (data[i+3] << 16) | (data[i+4] << 8) | data[i+5];
		}
	}
}

int mpeg2_aac_descriptor(const uint8_t* data, int bytes)
{
	int i;
	mpeg2_aac_descriptor_t desc;
	uint8_t descriptor_tag = data[0];
	uint8_t descriptor_len = data[1];

	i = 2;
	memset(&desc, 0, sizeof(desc));
	desc.profile = data[i];
	desc.channel_configuration = data[i+1];
	desc.additional_information = data[i+2];
	return i+2;
}

int mvc_extension_descriptor(const uint8_t* data, int bytes)
{
	int i;
	mvc_extension_descriptor_t desc;
	uint8_t descriptor_tag = data[0];
	uint8_t descriptor_len = data[1];

	i = 2;
	memset(&desc, 0, sizeof(desc));
	desc.average_bit_rate = (data[i] << 8) | data[i+1];
	desc.maximum_bitrate = (data[i+2] << 8) | data[i+3];
	desc.view_order_index_min = ((data[i+4] & 0xF) << 6) | ((data[i+5] >> 2) & 0x3F);
	desc.view_order_index_max = ((data[i+5] & 0x3) << 8) | data[i+6];
	desc.temporal_id_start = (data[i+7] >> 5) & 0x07;
	desc.temporal_id_end = (data[i+7] >> 2) & 0x07;
	desc.no_sei_nal_unit_present = (data[i+7] >> 1) & 0x01;
	desc.no_prefix_nal_unit_present = (data[i+7] >> 0) & 0x01;
	return i+7;
}
