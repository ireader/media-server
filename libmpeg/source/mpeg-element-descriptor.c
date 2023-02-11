// ITU-T H.222.0(06/2012)
// Information technology - Generic coding of moving pictures and associated audio information: Systems
// 2.6 Program and program element descriptors(p83)

#include "mpeg-element-descriptor.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

/*
2.6 Program and program element descriptors
2.6.1 Semantic definition of fields in program and program element descriptors
Table 2-45 - Program and program element descriptors
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
int mpeg_elment_descriptor(struct mpeg_bits_t* reader)
{
	size_t offset;
	uint8_t tag = mpeg_bits_read8(reader);
	uint8_t len = mpeg_bits_read8(reader);
	if (mpeg_bits_error(reader))
		return -1;

	offset = mpeg_bits_tell(reader);
	switch(tag)
	{
	case 2:
		video_stream_descriptor(reader, len);
		break;

	case 3:
		audio_stream_descriptor(reader, len);
		break;

	case 4:
		hierarchy_descriptor(reader, len);
		break;

	case 5:
		registration_descriptor(reader, len);
		break;

	case 10:
		language_descriptor(reader, len);
		break;

	case 11:
		system_clock_descriptor(reader, len);
		break;

	case 27:
		mpeg4_video_descriptor(reader, len);
		break;

	case 28:
		mpeg4_audio_descriptor(reader, len);
		break;

	case 37:
		metadata_pointer_descriptor(reader, len);
		break;

	case 38:
		metadata_descriptor(reader, len);
		break;

	case 40:
		avc_video_descriptor(reader, len);
		break;

	case 42:
		avc_timing_hrd_descriptor(reader, len);
		break;

	case 43:
		mpeg2_aac_descriptor(reader, len);
		break;

	case 48:
		svc_extension_descriptor(reader, len);
		break;

	case 49:
		mvc_extension_descriptor(reader, len);
		break;

	case 56:
		hevc_video_descriptor(reader, len);
		break;

	case 57:
		vvc_video_descriptor(reader, len);
		break;

	case 58:
		evc_video_descriptor(reader, len);
		break;

	case 0x40:
		clock_extension_descriptor(reader, len);
		break;

	//default:
	//	assert(0);
	}

	mpeg_bits_seek(reader, offset + len); // read all
	return mpeg_bits_error(reader) ? -1 : 0;
}

int video_stream_descriptor(struct mpeg_bits_t* reader, uint8_t len)
{
	// 2.6.2 Video stream descriptor(p85)

	uint8_t v;
	video_stream_descriptor_t desc;

	(void)len; assert(len >= 1);
	memset(&desc, 0, sizeof(desc));
	v = mpeg_bits_read8(reader);
	desc.multiple_frame_rate_flag = (v >> 7) & 0x01;
	desc.frame_rate_code = (v >> 3) & 0x0F;
	desc.MPEG_1_only_flag = (v >> 2) & 0x01;
	desc.constrained_parameter_flag = (v >> 1) & 0x01;
	desc.still_picture_flag = v & 0x01;

	if(0 == desc.MPEG_1_only_flag)
	{
		desc.profile_and_level_indication = mpeg_bits_read8(reader);
		v = mpeg_bits_read8(reader);
		desc.chroma_format = (v >> 6) & 0x03;
		desc.frame_rate_code = (v >> 5) & 0x01;
		assert((0x1F & v) == 0x00); // 'xxx00000'
	}

	assert(0 == mpeg_bits_error(reader));
	return mpeg_bits_error(reader) ? -1 : 0;
}

int audio_stream_descriptor(struct mpeg_bits_t* reader, uint8_t len)
{
	// 2.6.4 Audio stream descriptor(p86)

	uint8_t v;
	audio_stream_descriptor_t desc;

	(void)len; assert(len >= 1);
	v = mpeg_bits_read8(reader);
	memset(&desc, 0, sizeof(desc));
	desc.free_format_flag = (v >> 7) & 0x01;
	desc.ID = (v >> 6) & 0x01;
	desc.layer = (v >> 4) & 0x03;
	desc.variable_rate_audio_indicator = (v >> 3) & 0x01;

	assert(0 == mpeg_bits_error(reader));
	return mpeg_bits_error(reader) ? -1 : 0;
}

int hierarchy_descriptor(struct mpeg_bits_t* reader, uint8_t len)
{
	// 2.6.6 Hierarchy descriptor(p86)
	
	uint8_t v;
	hierarchy_descriptor_t desc;

	(void)len; assert(len >= 4);
	v = mpeg_bits_read8(reader);
	memset(&desc, 0, sizeof(desc));
	desc.no_view_scalability_flag = (v >> 7) & 0x01;
	desc.no_temporal_scalability_flag = (v >> 6) & 0x01;
	desc.no_spatial_scalability_flag = (v >> 5) & 0x01;
	desc.no_quality_scalability_flag = (v >> 4) & 0x01;
	desc.hierarchy_type = v & 0x0F;
	desc.hierarchy_layer_index = mpeg_bits_read8(reader) & 0x3F;
	v = mpeg_bits_read8(reader);
	desc.tref_present_flag = (v >> 7) & 0x01;
	desc.hierarchy_embedded_layer_index = v & 0x3F;
	desc.hierarchy_channel = mpeg_bits_read8(reader) & 0x3F;

	assert(0 == mpeg_bits_error(reader));
	return mpeg_bits_error(reader) ? -1 : 0;
}

int registration_descriptor(struct mpeg_bits_t* reader, uint8_t len)
{
	// 2.6.8 Registration descriptor(p94)
	size_t fourcc;

	(void)len; assert(len >= 4);
	fourcc = mpeg_bits_read32(reader);
	(void)fourcc;

	assert(0 == mpeg_bits_error(reader));
	return mpeg_bits_error(reader) ? -1 : 0;
}

int language_descriptor(struct mpeg_bits_t* reader, uint8_t len)
{
	// 2.6.18 ISO 639 language descriptor(p92)
	uint8_t i;
	uint32_t v;

	for (i = 0; i + 4 < len; i += 4)
	{
		language_descriptor_t desc;
		memset(&desc, 0, sizeof(desc));

		v = mpeg_bits_read32(reader);
		desc.code = v >> 8;
		desc.audio = v & 0xFF;
	}

	return mpeg_bits_error(reader) ? -1 : 0;
}

int system_clock_descriptor(struct mpeg_bits_t* reader, uint8_t len)
{
	// 2.6.20 System clock descriptor(p92)

	uint8_t v;
	system_clock_descriptor_t desc;

	(void)len; assert(len >= 2);
	v = mpeg_bits_read8(reader);
	memset(&desc, 0, sizeof(desc));
	desc.external_clock_reference_indicator = (v >> 7) & 0x01;
	desc.clock_accuracy_integer = v & 0x3F;
	desc.clock_accuracy_exponent = (mpeg_bits_read8(reader) >> 5) & 0x07;

	assert(0 == mpeg_bits_error(reader));
	return mpeg_bits_error(reader) ? -1 : 0;
}

int mpeg4_video_descriptor(struct mpeg_bits_t* reader, uint8_t len)
{
	// 2.6.36 MPEG-4 video descriptor(p96)

	mpeg4_video_descriptor_t desc;

	(void)len; assert(len >= 1);
	memset(&desc, 0, sizeof(desc));
	desc.visual_profile_and_level = mpeg_bits_read8(reader);

	assert(0 == mpeg_bits_error(reader));
	return mpeg_bits_error(reader) ? -1 : 0;
}

int mpeg4_audio_descriptor(struct mpeg_bits_t* reader, uint8_t len)
{
	// 2.6.38 MPEG-4 audio descriptor(p97)
	
	mpeg4_audio_descriptor_t desc;

	(void)len; assert(len >= 1);
	memset(&desc, 0, sizeof(desc));
	desc.profile_and_level = mpeg_bits_read8(reader);

	assert(0 == mpeg_bits_error(reader));
	return mpeg_bits_error(reader) ? -1 : 0;
}

int metadata_pointer_descriptor(struct mpeg_bits_t* reader, uint8_t len)
{
	// 2.6.58 Metadata pointer descriptor(p112)
	
	uint8_t flags;
	metadata_pointer_descriptor_t desc;

	(void)len; assert(len >= 5);
	desc.metadata_application_format_identifier = mpeg_bits_read16(reader);
	if (0xFFFF == desc.metadata_application_format_identifier)
		desc.metadata_application_format_identifier = mpeg_bits_read32(reader);

	desc.metadata_format_identifier = mpeg_bits_read8(reader);
	if (0xFF == desc.metadata_format_identifier)
		desc.metadata_format_identifier = mpeg_bits_read32(reader);

	desc.metadata_service_id = mpeg_bits_read8(reader);
	flags = mpeg_bits_read8(reader);
	desc.MPEG_carriage_flags = (flags >> 5) & 0x03;

	if (flags & 0x80) // metadata_locator_record_flag
	{
		desc.metadata_locator_record_length = mpeg_bits_read8(reader);
		mpeg_bits_skip(reader, desc.metadata_locator_record_length); // metadata_locator_record_byte
	}

	if (desc.MPEG_carriage_flags <= 2)
		desc.program_number = mpeg_bits_read16(reader);

	if (1 == desc.MPEG_carriage_flags)
	{
		desc.transport_stream_location = mpeg_bits_read16(reader);
		desc.transport_stream_id = mpeg_bits_read16(reader);
	}

	assert(0 == mpeg_bits_error(reader));
	return mpeg_bits_error(reader) ? -1 : 0;
}

int metadata_descriptor(struct mpeg_bits_t* reader, uint8_t len)
{
	// 2.6.60 Metadata descriptor(p115)

	uint8_t flags;
	metadata_descriptor_t desc;
	
	(void)len; assert(len >= 5);
	desc.metadata_application_format_identifier = mpeg_bits_read16(reader);
	if (0xFFFF == desc.metadata_application_format_identifier)
		desc.metadata_application_format_identifier = mpeg_bits_read32(reader);

	desc.metadata_format_identifier = mpeg_bits_read8(reader);
	if (0xFF == desc.metadata_format_identifier)
		desc.metadata_format_identifier = mpeg_bits_read32(reader);

	desc.metadata_service_id = mpeg_bits_read8(reader);
	flags = mpeg_bits_read8(reader);
	desc.decoder_config_flags = (flags >> 5) & 0x07;
	if (flags & 0x10) // DSM-CC_flag
	{
		desc.service_identification_length = mpeg_bits_read8(reader);
		mpeg_bits_skip(reader, desc.service_identification_length); // service_identification_record_byte
	}

	if (0x01 == desc.decoder_config_flags)
	{
		desc.decoder_config_length = mpeg_bits_read8(reader);
		mpeg_bits_skip(reader, desc.decoder_config_length); // decoder_config_byte
	}
	else if (0x03 == desc.decoder_config_flags)
	{
		desc.dec_config_identification_record_length = mpeg_bits_read8(reader);
		mpeg_bits_skip(reader, desc.dec_config_identification_record_length); // dec_config_identification_record_byte
	}
	else if (0x04 == desc.decoder_config_flags)
	{
		desc.decoder_config_metadata_service_id = mpeg_bits_read8(reader);
	}

	assert(0 == mpeg_bits_error(reader));
	return mpeg_bits_error(reader) ? -1 : 0;
}

int avc_video_descriptor(struct mpeg_bits_t* reader, uint8_t len)
{
	// 2.6.64 AVC video descriptor(p110)

	uint8_t v;
	avc_video_descriptor_t desc;
	
	(void)len; assert(len >= 4);
	memset(&desc, 0, sizeof(desc));
	desc.profile_idc = mpeg_bits_read8(reader);
	v = mpeg_bits_read8(reader);
	desc.constraint_set0_flag = (v >> 7) & 0x01;
	desc.constraint_set1_flag = (v >> 6) & 0x01;
	desc.constraint_set2_flag = (v >> 5) & 0x01;
	desc.constraint_set3_flag = (v >> 4) & 0x01;
	desc.constraint_set4_flag = (v >> 3) & 0x01;
	desc.constraint_set5_flag = (v >> 2) & 0x01;
	desc.AVC_compatible_flags = v & 0x03;
	desc.level_idc = mpeg_bits_read8(reader);
	v = mpeg_bits_read8(reader);
	desc.AVC_still_present = (v >> 7) & 0x01;
	desc.AVC_24_hour_picture_flag = (v >> 6) & 0x01;
	desc.frame_packing_SEI_not_present_flag = (v >> 5) & 0x01;

	assert(0 == mpeg_bits_error(reader));
	return mpeg_bits_error(reader) ? -1 : 0;
}

int avc_timing_hrd_descriptor(struct mpeg_bits_t* reader, uint8_t len)
{
	// 2.6.66 AVC timing and HRD descriptor(p112)

	uint8_t v;
	avc_timing_hrd_descriptor_t desc;

	(void)len; assert(len >= 2);
	memset(&desc, 0, sizeof(desc));
	v = mpeg_bits_read8(reader);
	desc.hrd_management_valid_flag = (v >> 7) & 0x01;
	desc.picture_and_timing_info_present = (v >> 0) & 0x01;
	if(desc.picture_and_timing_info_present)
	{
		v = mpeg_bits_read8(reader);
		desc._90kHZ_flag = (v >> 7) & 0x01;
		if(0 == desc._90kHZ_flag)
		{
			desc.N = mpeg_bits_read32(reader);
			desc.K = mpeg_bits_read32(reader);
		}
		desc.num_unit_in_tick = mpeg_bits_read32(reader);
	}

	v = mpeg_bits_read8(reader);
	desc.fixed_frame_rate_flag = (v >> 7) & 0x01;
	desc.temporal_poc_flag = (v >> 6) & 0x01;
	desc.picture_to_display_conversion_flag = (v >> 5) & 0x01;

	assert(0 == mpeg_bits_error(reader));
	return mpeg_bits_error(reader) ? -1 : 0;
}

int mpeg2_aac_descriptor(struct mpeg_bits_t* reader, uint8_t len)
{
	// 2.6.68 MPEG-2 AAC audio descriptor(p113)

	mpeg2_aac_descriptor_t desc;
	
	(void)len; assert(len >= 3);
	memset(&desc, 0, sizeof(desc));
	desc.profile = mpeg_bits_read8(reader);
	desc.channel_configuration = mpeg_bits_read8(reader);
	desc.additional_information = mpeg_bits_read8(reader);

	assert(0 == mpeg_bits_error(reader));
	return mpeg_bits_error(reader) ? -1 : 0;
}

int svc_extension_descriptor(struct mpeg_bits_t* reader, uint8_t len)
{
	// 2.6.76 SVC extension descriptor(p116)

	uint8_t v;
	svc_extension_descriptor_t desc;

	(void)len; assert(len >= 13);
	memset(&desc, 0, sizeof(desc));
	desc.width = mpeg_bits_read16(reader);
	desc.height = mpeg_bits_read16(reader);
	desc.frame_rate = mpeg_bits_read16(reader);
	desc.average_bitrate = mpeg_bits_read16(reader);
	desc.maximum_bitrate = mpeg_bits_read16(reader);
	desc.dependency_id = (mpeg_bits_read8(reader) >> 5) & 0x07;
	v = mpeg_bits_read8(reader);
	desc.quality_id_start = (v >> 4) & 0x0F;
	desc.quality_id_end = (v >> 0) & 0x0F;
	v = mpeg_bits_read8(reader);
	desc.temporal_id_start = (v >> 5) & 0x07;
	desc.temporal_id_end = (v >> 2) & 0x07;
	desc.no_sei_nal_unit_present = (v >> 1) & 0x01;

	assert(0 == mpeg_bits_error(reader));
	return mpeg_bits_error(reader) ? -1 : 0;
}

int mvc_extension_descriptor(struct mpeg_bits_t* reader, uint8_t len)
{
	// 2.6.78 MVC extension descriptor(p117)

	uint32_t v;
	mvc_extension_descriptor_t desc;

	(void)len; assert(len >= 8);
	memset(&desc, 0, sizeof(desc));
	desc.average_bit_rate = mpeg_bits_read16(reader);
	desc.maximum_bitrate = mpeg_bits_read16(reader);
	v = mpeg_bits_read32(reader);
	desc.view_order_index_min = (v >> 18) & 0x3FF;
	desc.view_order_index_max = (v >> 8) & 0x3FF;
	desc.temporal_id_start = (v >> 5) & 0x07;
	desc.temporal_id_end = (v >> 2) & 0x07;
	desc.no_sei_nal_unit_present = (v >> 1) & 0x01;
	desc.no_prefix_nal_unit_present = (v >> 0) & 0x01;

	assert(0 == mpeg_bits_error(reader));
	return mpeg_bits_error(reader) ? -1 : 0;
}

int hevc_video_descriptor(struct mpeg_bits_t* reader, uint8_t len)
{
	// 2.6.95 HEVC video descriptor(p146)

	uint64_t v;
	hevc_video_descriptor_t desc;

	(void)len; assert(len >= 13);
	memset(&desc, 0, sizeof(desc));
	v = mpeg_bits_read8(reader);
	desc.profile_space = (v >> 6) & 0x03;
	desc.tier_flag = (v >> 5) & 0x01;
	desc.profile_idc = (v >> 0) & 0x1F;
	desc.profile_compatibility_indication = mpeg_bits_read32(reader);
	v = mpeg_bits_read64(reader);
	desc.progressive_source_flag = (v >> 63) & 0x01;
	desc.interlaced_source_flag = (v >> 62) & 0x01;
	desc.non_packed_constraint_flag = (v >> 61) & 0x01;
	desc.frame_only_constraint_flag = (v >> 60) & 0x01;
	desc.copied_44bits = (v >> 16) & 0xFFFFFFFFFFFULL;
	desc.level_idc = (v >> 8) & 0xFF;
	desc.temporal_layer_subset_flag = (v >> 7) & 0x01;
	desc.HEVC_still_present_flag = (v >> 6) & 0x01;
	desc.HEVC_24hr_picture_present_flag = (v >> 5) & 0x01;
	desc.sub_pic_hrd_params_not_present_flag = (v >> 4) & 0x01;
	desc.HDR_WCG_idc = v & 0x03;
	if (desc.temporal_layer_subset_flag) {
		v = mpeg_bits_read8(reader);
		desc.temporal_id_min = (v >> 5) & 0x07;
		v = mpeg_bits_read8(reader);
		desc.temporal_id_max = (v >> 5) & 0x07;
	}

	assert(0 == mpeg_bits_error(reader));
	return mpeg_bits_error(reader) ? -1 : 0;
}

int vvc_video_descriptor(struct mpeg_bits_t* reader, uint8_t len)
{
	// 2.6.129 VVC video descriptor(p172)

	int i;
	uint8_t v;
	vvc_video_descriptor_t desc;

	(void)len; assert(len >= 6);
	memset(&desc, 0, sizeof(desc));
	v = mpeg_bits_read8(reader);
	desc.profile_idc = (v >> 1) & 0x7F;
	desc.tier_flag = v & 0x01;
	desc.num_sub_profiles = mpeg_bits_read8(reader);
	for(i = 0; i < desc.num_sub_profiles && i < sizeof(desc.sub_profile_idc)/sizeof(desc.sub_profile_idc[0]); i++)
		desc.sub_profile_idc[i] = mpeg_bits_read32(reader);

	v = mpeg_bits_read8(reader);
	desc.progressive_source_flag = (v >> 7) & 0x01;
	desc.interlaced_source_flag = (v >> 6) & 0x01;
	desc.non_packed_constraint_flag = (v >> 5) & 0x01;
	desc.frame_only_constraint_flag = (v >> 4) & 0x01;
	desc.reserved_zero_4bits = (v >> 0) & 0x0F;
	desc.level_idc = mpeg_bits_read8(reader);

	v = mpeg_bits_read8(reader);
	desc.temporal_layer_subset_flag = (v >> 7) & 0x01;
	desc.VVC_still_present_flag = (v >> 6) & 0x01;
	desc.VVC_24hr_picture_present_flag = (v >> 5) & 0x01;

	v = mpeg_bits_read8(reader);
	desc.HDR_WCG_idc = (v >> 6) & 0x03;
	desc.video_properties_tag = v & 0x0F;

	if (desc.temporal_layer_subset_flag) {
		v = mpeg_bits_read8(reader);
		desc.temporal_id_min = v & 0x07;
		v = mpeg_bits_read8(reader);
		desc.temporal_id_max = v & 0x07;
	}

	assert(0 == mpeg_bits_error(reader));
	return mpeg_bits_error(reader) ? -1 : 0;
}

int evc_video_descriptor(struct mpeg_bits_t* reader, uint8_t len)
{
	// 2.6.133 EVC video descriptor(p176)

	uint8_t v;
	evc_video_descriptor_t desc;

	(void)len; assert(len >= 6);
	memset(&desc, 0, sizeof(desc));
	desc.profile_idc = mpeg_bits_read8(reader);
	desc.level_idc = mpeg_bits_read8(reader);
	desc.toolset_idc_h = mpeg_bits_read32(reader);
	desc.toolset_idc_l = mpeg_bits_read32(reader);

	v = mpeg_bits_read8(reader);
	desc.progressive_source_flag = (v >> 7) & 0x01;
	desc.interlaced_source_flag = (v >> 6) & 0x01;
	desc.non_packed_constraint_flag = (v >> 5) & 0x01;
	desc.frame_only_constraint_flag = (v >> 4) & 0x01;
	desc.reserved = (v >> 3) & 0x01;
	desc.temporal_layer_subset_flag = (v >> 2) & 0x01;
	desc.EVC_still_present_flag = (v >> 1) & 0x01;
	desc.EVC_24hr_picture_present_flag = (v >> 0) & 0x01;

	v = mpeg_bits_read8(reader);
	desc.HDR_WCG_idc = (v >> 6) & 0x03;
	desc.video_properties_tag = v & 0x0F;

	if (desc.temporal_layer_subset_flag) {
		v = mpeg_bits_read8(reader);
		desc.temporal_id_min = v & 0x07;
		v = mpeg_bits_read8(reader);
		desc.temporal_id_max = v & 0x07;
	}

	assert(0 == mpeg_bits_error(reader));
	return mpeg_bits_error(reader) ? -1 : 0;
}

size_t service_extension_descriptor_write(uint8_t* data, size_t bytes)
{
	uint8_t n;
	n = (uint8_t)strlen(SERVICE_NAME);
	if (bytes < 2 + n)
		return 0;
	data[0] = SERVICE_ID;
	data[1] = 2 + n;
	memcpy(data + 2, SERVICE_NAME, n);
	return 2 + n;
}

typedef struct _clock_extension_descriptor_t
{
	uint8_t year; // base 2000, 8-bit
	uint8_t month; // 1-12, 4-bit
	uint8_t day; // 1-31, 5-bit
	uint8_t hour; // 0-23, 5-bit
	uint8_t minute; // 0-59, 6-bit
	uint8_t second; // 0-59, 6-bit
	uint16_t microsecond; // 14-bit
} clock_extension_descriptor_t;

int clock_extension_descriptor(struct mpeg_bits_t* reader, uint8_t len)
{
	uint32_t v;
	struct tm t;
	time_t clock;
	
	(void)len; assert(len >= 9);
	v = mpeg_bits_read32(reader); // skip 4-bytes leading
	memset(&t, 0, sizeof(t));
	t.tm_year = mpeg_bits_read8(reader) + 2000 - 1900;
	v = mpeg_bits_read32(reader);
	t.tm_mon = ((v >> 28) & 0x0F) - 1;
	t.tm_mday = (v >> 23) & 0x1F;
	t.tm_hour = (v >> 18) & 0x1F;
	t.tm_min = (v >> 12) & 0x3F;
	t.tm_sec = (v >> 6) & 0x3F;
	//desc.microsecond = v & 0x3F;
	clock = mktime(&t) * 1000;

	assert(0 == mpeg_bits_error(reader));
	return mpeg_bits_error(reader) ? -1 : 0;
}

size_t clock_extension_descriptor_write(uint8_t* data, size_t bytes, int64_t clock)
{
	struct tm* t;
	time_t seconds;
	if (bytes < 16)
		return 0;

	seconds = (time_t)(clock / 1000);
	t = localtime(&seconds);

	data[0] = 0x40;
	data[1] = 0x0E;
	data[2] = 0x48;
	data[3] = 0x4B;
	data[4] = 0x01;
	data[5] = 0x00;
	data[6] = (uint8_t)(t->tm_year + 1900 - 2000); // base 2000
	data[7] = (uint8_t)((t->tm_mon + 1) << 4) | ((t->tm_mday >> 1) & 0x0F);
	data[8] = (uint8_t)((t->tm_mday & 0x01) << 7) | ((t->tm_hour & 0x1F) << 2) | ((t->tm_min >> 4) & 0x03);
	data[9] = (uint8_t)((t->tm_min & 0x0F) << 4) | ((t->tm_sec >> 2) & 0x0F);
	data[10] = (uint8_t)((t->tm_sec & 0x03) << 6);
	data[11] = 0x00;
	data[12] = 0x00;
	data[13] = 0xFF;
	data[14] = 0xFF;
	data[15] = 0xFF;
	return 16;
}
