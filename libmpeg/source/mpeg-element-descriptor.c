// ITU-T H.222.0(06/2012)
// Information technology ¨C Generic coding of moving pictures and associated audio information: Systems
// 2.6 Program and program element descriptors(p83)

#include "mpeg-ps-proto.h"
#include "mpeg-element-descriptor.h"
#include <string.h>
#include <assert.h>

/*
2.6 Program and program element descriptors
2.6.1 Semantic definition of fields in program and program element descriptors
Table 2-45 ¨C Program and program element descriptors
tag		TS	PS	Identification
0		n/a n/a reserved
1		n/a X	forbidden
2		X	X	video_stream_descriptor
3		X	X	audio_stream_descriptor
4		X	X	hierarchy_descriptor
5		X	X	registration_descriptor
6		X	X	data_stream_alignment_descriptor
7		X	X	target_background_grid_descriptor
8		X	X	video_window_descriptor
9		X	X	CA_descriptor
10		X	X	ISO_639_language_descriptor
11		X	X	system_clock_descriptor
12		X	X	multiplex_buffer_utilization_descriptor
13		X	X	copyright_descriptor
14		X		maximum_bitrate_descriptor
15		X	X	private_data_indicator_descriptor
16		X	X	smoothing_buffer_descriptor
17		X		STD_descriptor
18		X	X	IBP_descriptor
19-26	X		Defined in ISO/IEC 13818-6
27		X	X	MPEG-4_video_descriptor
28		X	X	MPEG-4_audio_descriptor
29		X	X	IOD_descriptor
30		X		SL_descriptor
31		X	X	FMC_descriptor
32		X	X	external_ES_ID_descriptor
33		X	X	MuxCode_descriptor
34		X	X	FmxBufferSize_descriptor
35		X		multiplexbuffer_descriptor
36		X	X	content_labeling_descriptor
37		X	X	metadata_pointer_descriptor
38		X	X	metadata_descriptor
39		X	X	metadata_STD_descriptor
40		X	X	AVC video descriptor
41		X	X	IPMP_descriptor (defined in ISO/IEC 13818-11, MPEG-2 IPMP)
42		X	X	AVC timing and HRD descriptor
43		X	X	MPEG-2_AAC_audio_descriptor
44		X	X	FlexMuxTiming_descriptor
45		X	X	MPEG-4_text_descriptor
46		X	X	MPEG-4_audio_extension_descriptor
47		X	X	auxiliary_video_stream_descriptor
48		X	X	SVC extension descriptor
49		X	X	MVC extension descriptor
50		X	n/a J2K video descriptor
51		X	X	MVC operation point descriptor
52		X	X	MPEG2_stereoscopic_video_format_descriptor
53		X	X	Stereoscopic_program_info_descriptor
54		X	X	Stereoscopic_video_info_descriptor
55      X   n/a Transport_profile_descriptor
56      X   n/a HEVC video descriptor
57-63	n/a n/a Rec. ITU-T H.222.0 | ISO/IEC 13818-1 Reserved
64-255	n/a n/a User Private
*/
size_t mpeg_elment_descriptor(const uint8_t* data, size_t bytes)
{
	uint8_t descriptor_tag = data[0];
	uint8_t descriptor_len = data[1];
	if (descriptor_len + 2 > bytes)
		return bytes;

	switch(descriptor_tag)
	{
	case 2:
		video_stream_descriptor(data, bytes);
		break;

	case 3:
		audio_stream_descriptor(data, bytes);
		break;

	case 4:
		hierarchy_descriptor(data, bytes);
		break;

	case 10:
		language_descriptor(data, bytes);
		break;

	case 11:
		system_clock_descriptor(data, bytes);
		break;

	case 27:
		mpeg4_video_descriptor(data, bytes);
		break;

	case 28:
		mpeg4_audio_descriptor(data, bytes);
		break;

	case 40:
		avc_video_descriptor(data, bytes);
		break;

	case 42:
		avc_timing_hrd_descriptor(data, bytes);
		break;

	case 43:
		mpeg2_aac_descriptor(data, bytes);
		break;

	case 48:
		svc_extension_descriptor(data, bytes);
		break;

	case 49:
		mvc_extension_descriptor(data, bytes);
		break;

	//default:
	//	assert(0);
	}

	return descriptor_len+2;
}

size_t video_stream_descriptor(const uint8_t* data, size_t bytes)
{
	// 2.6.2 Video stream descriptor(p85)
	size_t i;
	video_stream_descriptor_t desc;
//	uint8_t descriptor_tag = data[0];
	size_t descriptor_len = data[1];
	assert(descriptor_len+2 <= bytes);

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

	return descriptor_len+2;
}

size_t audio_stream_descriptor(const uint8_t* data, size_t bytes)
{
	// 2.6.4 Audio stream descriptor(p86)
	size_t i;
	audio_stream_descriptor_t desc;
//	uint8_t descriptor_tag = data[0];
	size_t descriptor_len = data[1];
	assert(descriptor_len+2 <= bytes);

	i = 2;
	memset(&desc, 0, sizeof(desc));
	desc.free_format_flag = (data[i] >> 7) & 0x01;
	desc.ID = (data[i] >> 6) & 0x01;
	desc.layer = (data[i] >> 4) & 0x03;
	desc.variable_rate_audio_indicator = (data[i] >> 3) & 0x01;

	assert(4 == descriptor_len);
	return descriptor_len+2;
}

size_t hierarchy_descriptor(const uint8_t* data, size_t bytes)
{
	// 2.6.6 Hierarchy descriptor(p86)
	size_t i;
	hierarchy_descriptor_t desc;
//	uint8_t descriptor_tag = data[0];
	size_t descriptor_len = data[1];
	assert(descriptor_len+2 <= bytes);

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

	assert(4 == descriptor_len);
	return descriptor_len+2;
}

size_t language_descriptor(const uint8_t* data, size_t bytes)
{
	// 2.6.18 ISO 639 language descriptor(p92)
	size_t i;
//	uint8_t descriptor_tag = data[0];
	size_t descriptor_len = data[1];
	assert(descriptor_len+2 <= bytes);

	for(i = 2; i < descriptor_len; i += 4)
	{
		language_descriptor_t desc;
		memset(&desc, 0, sizeof(desc));

		desc.code = (data[i] << 16) | (data[i+1] << 8) | data[i+2];
		desc.audio = data[i+3];
	}

	return descriptor_len+2;
}

size_t system_clock_descriptor(const uint8_t* data, size_t bytes)
{
	// 2.6.20 System clock descriptor(p92)
	size_t i;
	system_clock_descriptor_t desc;
//	uint8_t descriptor_tag = data[0];
	size_t descriptor_len = data[1];
	assert(descriptor_len+2 <= bytes);

	i = 2;
	memset(&desc, 0, sizeof(desc));
	desc.external_clock_reference_indicator = (data[i] >> 7) & 0x01;
	desc.clock_accuracy_integer = data[i] & 0x3F;
	desc.clock_accuracy_exponent = (data[i+1] >> 5) & 0x07;

	assert(2 == descriptor_len);
	return descriptor_len+2;
}

size_t mpeg4_video_descriptor(const uint8_t* data, size_t bytes)
{
	// 2.6.36 MPEG-4 video descriptor(p96)
	size_t i;
	mpeg4_video_descriptor_t desc;
//	uint8_t descriptor_tag = data[0];
	size_t descriptor_len = data[1];
	assert(descriptor_len+2 <= bytes);

	i = 2;
	memset(&desc, 0, sizeof(desc));
	desc.visual_profile_and_level = data[i];

	assert(1 == descriptor_len);
	return descriptor_len+2;
}

size_t mpeg4_audio_descriptor(const uint8_t* data, size_t bytes)
{
	// 2.6.38 MPEG-4 audio descriptor(p97)
	size_t i;
	mpeg4_audio_descriptor_t desc;
//	uint8_t descriptor_tag = data[0];
	size_t descriptor_len = data[1];
	assert(descriptor_len+2 <= bytes);

	i = 2;
	memset(&desc, 0, sizeof(desc));
	desc.profile_and_level = data[i];

	assert(1 == descriptor_len);
	return descriptor_len+2;
}

size_t avc_video_descriptor(const uint8_t* data, size_t bytes)
{
	// 2.6.64 AVC video descriptor(p110)
	size_t i;
	avc_video_descriptor_t desc;
//	uint8_t descriptor_tag = data[0];
	size_t descriptor_len = data[1];
	assert(descriptor_len+2 <= bytes);

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

	assert(4 == descriptor_len);
	return descriptor_len+2;
}

size_t avc_timing_hrd_descriptor(const uint8_t* data, size_t bytes)
{
	// 2.6.66 AVC timing and HRD descriptor(p112)
	size_t i;
	avc_timing_hrd_descriptor_t desc;
//	uint8_t descriptor_tag = data[0];
	size_t descriptor_len = data[1];
	assert(descriptor_len+2 <= bytes);

	i = 2;
	memset(&desc, 0, sizeof(desc));
	desc.hrd_management_valid_flag = (data[i] >> 7) & 0x01;
	desc.picture_and_timing_info_present = (data[i] >> 0) & 0x01;
	++i;
	if(desc.picture_and_timing_info_present)
	{
		desc._90kHZ_flag = (data[i] >> 7) & 0x01;
		if(0 == desc._90kHZ_flag)
		{
			desc.N = (data[i+1] << 24) | (data[i+2] << 16) | (data[i+3] << 8) | data[i+4];
			desc.K = (data[i+5] << 24) | (data[i+6] << 16) | (data[i+7] << 8) | data[i+8];
			i += 8;
		}
		desc.num_unit_in_tick = (data[i+1] << 24) | (data[i+2] << 16) | (data[i+3] << 8) | data[i+4];
		i += 5;
	}

	desc.fixed_frame_rate_flag = (data[i] >> 7) & 0x01;
	desc.temporal_poc_flag = (data[i] >> 6) & 0x01;
	desc.picture_to_display_conversion_flag = (data[i] >> 5) & 0x01;

	return descriptor_len+2;
}

size_t mpeg2_aac_descriptor(const uint8_t* data, size_t bytes)
{
	// 2.6.68 MPEG-2 AAC audio descriptor(p113)
	size_t i;
	mpeg2_aac_descriptor_t desc;
//	uint8_t descriptor_tag = data[0];
	size_t descriptor_len = data[1];
	assert(descriptor_len+2 <= bytes);

	i = 2;
	memset(&desc, 0, sizeof(desc));
	desc.profile = data[i];
	desc.channel_configuration = data[i+1];
	desc.additional_information = data[i+2];

	assert(3 == descriptor_len);
	return descriptor_len+2;
}

size_t svc_extension_descriptor(const uint8_t* data, size_t bytes)
{
	// 2.6.76 SVC extension descriptor(p116)
	size_t i;
	svc_extension_descriptor_t desc;
//	uint8_t descriptor_tag = data[0];
	size_t descriptor_len = data[1];
	assert(descriptor_len+2 <= bytes);

	i = 2;
	memset(&desc, 0, sizeof(desc));
	desc.width = (data[i] << 8) | data[i+1];
	desc.height = (data[i+2] << 8) | data[i+3];
	desc.frame_rate = (data[i+4] << 8) | data[i+5];
	desc.average_bitrate = (data[i+6] << 8) | data[i+7];
	desc.maximum_bitrate = (data[i+8] << 8) | data[i+9];
	desc.dependency_id = (data[i+10] >> 5) & 0x07;
	desc.quality_id_start = (data[i+11] >> 4) & 0x0F;
	desc.quality_id_end = (data[i+11] >> 0) & 0x0F;
	desc.temporal_id_start = (data[i+12] >> 5) & 0x07;
	desc.temporal_id_end = (data[i+12] >> 2) & 0x07;
	desc.no_sei_nal_unit_present = (data[i+12] >> 1) & 0x01;

	assert(13 == descriptor_len);
	return descriptor_len+2;
}

size_t mvc_extension_descriptor(const uint8_t* data, size_t bytes)
{
	// 2.6.78 MVC extension descriptor(p117)
	size_t i;
	mvc_extension_descriptor_t desc;
//	uint8_t descriptor_tag = data[0];
	size_t descriptor_len = data[1];
	assert(descriptor_len+2 <= bytes);

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

	assert(8 == descriptor_len);
	return descriptor_len+2;
}
