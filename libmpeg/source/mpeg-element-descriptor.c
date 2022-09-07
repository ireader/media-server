// ITU-T H.222.0(06/2012)
// Information technology - Generic coding of moving pictures and associated audio information: Systems
// 2.6 Program and program element descriptors(p83)

#include "mpeg-ps-proto.h"
#include "mpeg-element-descriptor.h"
#include "mpeg-util.h"
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
size_t mpeg_elment_descriptor(const uint8_t* data, size_t bytes)
{
	uint8_t descriptor_tag = data[0];
	uint8_t descriptor_len = data[1];
	if ((size_t)descriptor_len + 2 > bytes)
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

	case 5:
		registration_descriptor(data, bytes);
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

	case 37:
		metadata_pointer_descriptor(data, bytes);
		break;

	case 38:
		metadata_descriptor(data, bytes);
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

	case 0x40:
		clock_extension_descriptor(data, bytes);
		break;

	//default:
	//	assert(0);
	}

	return descriptor_len+2;
}

size_t video_stream_descriptor(const uint8_t* data, size_t bytes)
{
	// 2.6.2 Video stream descriptor(p85)

	uint8_t v;
	size_t descriptor_len;
	struct mpeg_bits_t bits;
	video_stream_descriptor_t desc;

	mpeg_bits_init(&bits, data, bytes);
	mpeg_bits_read8(&bits); // descriptor_tag
	descriptor_len = mpeg_bits_read8(&bits);
	assert(descriptor_len + 2 <= bytes);

	memset(&desc, 0, sizeof(desc));
	v = mpeg_bits_read8(&bits);
	desc.multiple_frame_rate_flag = (v >> 7) & 0x01;
	desc.frame_rate_code = (v >> 3) & 0x0F;
	desc.MPEG_1_only_flag = (v >> 2) & 0x01;
	desc.constrained_parameter_flag = (v >> 1) & 0x01;
	desc.still_picture_flag = v & 0x01;

	if(0 == desc.MPEG_1_only_flag)
	{
		desc.profile_and_level_indication = mpeg_bits_read8(&bits);
		v = mpeg_bits_read8(&bits);
		desc.chroma_format = (v >> 6) & 0x03;
		desc.frame_rate_code = (v >> 5) & 0x01;
		assert((0x1F & v) == 0x00); // 'xxx00000'
	}

	assert(0 == mpeg_bits_error(&bits));
	return descriptor_len+2;
}

size_t audio_stream_descriptor(const uint8_t* data, size_t bytes)
{
	// 2.6.4 Audio stream descriptor(p86)

	uint8_t v;
	size_t descriptor_len;
	struct mpeg_bits_t bits;
	audio_stream_descriptor_t desc;

	mpeg_bits_init(&bits, data, bytes);
	mpeg_bits_read8(&bits); // descriptor_tag
	descriptor_len = mpeg_bits_read8(&bits);
	assert(descriptor_len + 2 <= bytes);

	v = mpeg_bits_read8(&bits);
	memset(&desc, 0, sizeof(desc));
	desc.free_format_flag = (v >> 7) & 0x01;
	desc.ID = (v >> 6) & 0x01;
	desc.layer = (v >> 4) & 0x03;
	desc.variable_rate_audio_indicator = (v >> 3) & 0x01;

	assert(0 == mpeg_bits_error(&bits));
	return descriptor_len+2;
}

size_t hierarchy_descriptor(const uint8_t* data, size_t bytes)
{
	// 2.6.6 Hierarchy descriptor(p86)
	
	uint8_t v;
	size_t descriptor_len;
	struct mpeg_bits_t bits;
	hierarchy_descriptor_t desc;
	
	mpeg_bits_init(&bits, data, bytes);
	mpeg_bits_read8(&bits); // descriptor_tag
	descriptor_len = mpeg_bits_read8(&bits);
	assert(descriptor_len + 2 <= bytes);

	v = mpeg_bits_read8(&bits);
	memset(&desc, 0, sizeof(desc));
	desc.no_view_scalability_flag = (v >> 7) & 0x01;
	desc.no_temporal_scalability_flag = (v >> 6) & 0x01;
	desc.no_spatial_scalability_flag = (v >> 5) & 0x01;
	desc.no_quality_scalability_flag = (v >> 4) & 0x01;
	desc.hierarchy_type = v & 0x0F;
	desc.hierarchy_layer_index = mpeg_bits_read8(&bits) & 0x3F;
	v = mpeg_bits_read8(&bits);
	desc.tref_present_flag = (v >> 7) & 0x01;
	desc.hierarchy_embedded_layer_index = v & 0x3F;
	desc.hierarchy_channel = mpeg_bits_read8(&bits) & 0x3F;

	assert(0 == mpeg_bits_error(&bits));
	return descriptor_len+2;
}

size_t registration_descriptor(const uint8_t* data, size_t bytes)
{
	// 2.6.8 Registration descriptor(p94)
	size_t fourcc;
	size_t descriptor_len;
	struct mpeg_bits_t bits;

	mpeg_bits_init(&bits, data, bytes);
	mpeg_bits_read8(&bits); // descriptor_tag
	descriptor_len = mpeg_bits_read8(&bits);
	assert(descriptor_len + 2 <= bytes);

	fourcc = mpeg_bits_read32(&bits);
	(void)fourcc;

	assert(0 == mpeg_bits_error(&bits));
	return descriptor_len + 2;
}

size_t language_descriptor(const uint8_t* data, size_t bytes)
{
	// 2.6.18 ISO 639 language descriptor(p92)
	size_t i;
//	uint8_t descriptor_tag = data[0];
	size_t descriptor_len = data[1];
	assert(descriptor_len+2 <= bytes);

	for(i = 2; i + 4 <= descriptor_len + 2; i += 4)
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

	uint8_t v;
	size_t descriptor_len;
	struct mpeg_bits_t bits;
	system_clock_descriptor_t desc;

	mpeg_bits_init(&bits, data, bytes);
	mpeg_bits_read8(&bits); // descriptor_tag
	descriptor_len = mpeg_bits_read8(&bits);
	assert(descriptor_len + 2 <= bytes);

	v = mpeg_bits_read8(&bits);
	memset(&desc, 0, sizeof(desc));
	desc.external_clock_reference_indicator = (v >> 7) & 0x01;
	desc.clock_accuracy_integer = v & 0x3F;
	desc.clock_accuracy_exponent = (mpeg_bits_read8(&bits) >> 5) & 0x07;

	assert(0 == mpeg_bits_error(&bits));
	return descriptor_len+2;
}

size_t mpeg4_video_descriptor(const uint8_t* data, size_t bytes)
{
	// 2.6.36 MPEG-4 video descriptor(p96)

	size_t descriptor_len;
	struct mpeg_bits_t bits;
	mpeg4_video_descriptor_t desc;

	mpeg_bits_init(&bits, data, bytes);
	mpeg_bits_read8(&bits); // descriptor_tag
	descriptor_len = mpeg_bits_read8(&bits);
	assert(descriptor_len + 2 <= bytes);

	memset(&desc, 0, sizeof(desc));
	desc.visual_profile_and_level = mpeg_bits_read8(&bits);

	assert(0 == mpeg_bits_error(&bits));
	return descriptor_len+2;
}

size_t mpeg4_audio_descriptor(const uint8_t* data, size_t bytes)
{
	// 2.6.38 MPEG-4 audio descriptor(p97)
	
	size_t descriptor_len;
	struct mpeg_bits_t bits;
	mpeg4_audio_descriptor_t desc;

	mpeg_bits_init(&bits, data, bytes);
	mpeg_bits_read8(&bits); // descriptor_tag
	descriptor_len = mpeg_bits_read8(&bits);
	assert(descriptor_len + 2 <= bytes);

	memset(&desc, 0, sizeof(desc));
	desc.profile_and_level = mpeg_bits_read8(&bits);

	assert(0 == mpeg_bits_error(&bits));
	return descriptor_len+2;
}

size_t metadata_pointer_descriptor(const uint8_t* data, size_t bytes)
{
	// 2.6.58 Metadata pointer descriptor(p112)
	
	uint8_t flags;
	struct mpeg_bits_t bits;
	metadata_pointer_descriptor_t desc;
	size_t descriptor_len;

	mpeg_bits_init(&bits, data, bytes);
	mpeg_bits_read8(&bits); // descriptor_tag
	descriptor_len = mpeg_bits_read8(&bits);
	assert(descriptor_len + 2 <= bytes);

	desc.metadata_application_format_identifier = mpeg_bits_read16(&bits);
	if (0xFFFF == desc.metadata_application_format_identifier)
		desc.metadata_application_format_identifier = mpeg_bits_read32(&bits);

	desc.metadata_format_identifier = mpeg_bits_read8(&bits);
	if (0xFF == desc.metadata_format_identifier)
		desc.metadata_format_identifier = mpeg_bits_read32(&bits);

	desc.metadata_service_id = mpeg_bits_read8(&bits);
	flags = mpeg_bits_read8(&bits);
	desc.MPEG_carriage_flags = (flags >> 5) & 0x03;

	if (flags & 0x80) // metadata_locator_record_flag
	{
		desc.metadata_locator_record_length = mpeg_bits_read8(&bits);
		mpeg_bits_skip(&bits, desc.metadata_locator_record_length); // metadata_locator_record_byte
	}

	if (desc.MPEG_carriage_flags <= 2)
		desc.program_number = mpeg_bits_read16(&bits);

	if (1 == desc.MPEG_carriage_flags)
	{
		desc.transport_stream_location = mpeg_bits_read16(&bits);
		desc.transport_stream_id = mpeg_bits_read16(&bits);
	}

	assert(0 == mpeg_bits_error(&bits));
	return descriptor_len + 2;
}

size_t metadata_descriptor(const uint8_t* data, size_t bytes)
{
	// 2.6.60 Metadata descriptor(p115)

	uint8_t flags;
	struct mpeg_bits_t bits;
	metadata_descriptor_t desc;
	size_t descriptor_len;

	mpeg_bits_init(&bits, data, bytes);
	mpeg_bits_read8(&bits); // descriptor_tag
	descriptor_len = mpeg_bits_read8(&bits);
	assert(descriptor_len + 2 <= bytes);

	desc.metadata_application_format_identifier = mpeg_bits_read16(&bits);
	if (0xFFFF == desc.metadata_application_format_identifier)
		desc.metadata_application_format_identifier = mpeg_bits_read32(&bits);

	desc.metadata_format_identifier = mpeg_bits_read8(&bits);
	if (0xFF == desc.metadata_format_identifier)
		desc.metadata_format_identifier = mpeg_bits_read32(&bits);

	desc.metadata_service_id = mpeg_bits_read8(&bits);
	flags = mpeg_bits_read8(&bits);
	desc.decoder_config_flags = (flags >> 5) & 0x07;
	if (flags & 0x10) // DSM-CC_flag
	{
		desc.service_identification_length = mpeg_bits_read8(&bits);
		mpeg_bits_skip(&bits, desc.service_identification_length); // service_identification_record_byte
	}

	if (0x01 == desc.decoder_config_flags)
	{
		desc.decoder_config_length = mpeg_bits_read8(&bits);
		mpeg_bits_skip(&bits, desc.decoder_config_length); // decoder_config_byte
	}
	else if (0x03 == desc.decoder_config_flags)
	{
		desc.dec_config_identification_record_length = mpeg_bits_read8(&bits);
		mpeg_bits_skip(&bits, desc.dec_config_identification_record_length); // dec_config_identification_record_byte
	}
	else if (0x04 == desc.decoder_config_flags)
	{
		desc.decoder_config_metadata_service_id = mpeg_bits_read8(&bits);
	}

	assert(0 == mpeg_bits_error(&bits));
	return descriptor_len + 2;
}

size_t avc_video_descriptor(const uint8_t* data, size_t bytes)
{
	// 2.6.64 AVC video descriptor(p110)

	uint8_t v;
	size_t descriptor_len;
	struct mpeg_bits_t bits;
	avc_video_descriptor_t desc;
	
	mpeg_bits_init(&bits, data, bytes);
	mpeg_bits_read8(&bits); // descriptor_tag
	descriptor_len = mpeg_bits_read8(&bits);
	assert(descriptor_len + 2 <= bytes);

	memset(&desc, 0, sizeof(desc));
	desc.profile_idc = mpeg_bits_read8(&bits);
	v = mpeg_bits_read8(&bits);
	desc.constraint_set0_flag = (v >> 7) & 0x01;
	desc.constraint_set1_flag = (v >> 6) & 0x01;
	desc.constraint_set2_flag = (v >> 5) & 0x01;
	desc.constraint_set3_flag = (v >> 4) & 0x01;
	desc.constraint_set4_flag = (v >> 3) & 0x01;
	desc.constraint_set5_flag = (v >> 2) & 0x01;
	desc.AVC_compatible_flags = v & 0x03;
	desc.level_idc = mpeg_bits_read8(&bits);
	v = mpeg_bits_read8(&bits);
	desc.AVC_still_present = (v >> 7) & 0x01;
	desc.AVC_24_hour_picture_flag = (v >> 6) & 0x01;
	desc.frame_packing_SEI_not_present_flag = (v >> 5) & 0x01;

	assert(0 == mpeg_bits_error(&bits));
	return descriptor_len+2;
}

size_t avc_timing_hrd_descriptor(const uint8_t* data, size_t bytes)
{
	// 2.6.66 AVC timing and HRD descriptor(p112)

	uint8_t v;
	size_t descriptor_len;
	struct mpeg_bits_t bits;
	avc_timing_hrd_descriptor_t desc;

	mpeg_bits_init(&bits, data, bytes);
	mpeg_bits_read8(&bits); // descriptor_tag
	descriptor_len = mpeg_bits_read8(&bits);
	assert(descriptor_len + 2 <= bytes);

	memset(&desc, 0, sizeof(desc));
	v = mpeg_bits_read8(&bits);
	desc.hrd_management_valid_flag = (v >> 7) & 0x01;
	desc.picture_and_timing_info_present = (v >> 0) & 0x01;
	if(desc.picture_and_timing_info_present)
	{
		v = mpeg_bits_read8(&bits);
		desc._90kHZ_flag = (v >> 7) & 0x01;
		if(0 == desc._90kHZ_flag)
		{
			desc.N = mpeg_bits_read32(&bits);
			desc.K = mpeg_bits_read32(&bits);
		}
		desc.num_unit_in_tick = mpeg_bits_read32(&bits);
	}

	v = mpeg_bits_read8(&bits);
	desc.fixed_frame_rate_flag = (v >> 7) & 0x01;
	desc.temporal_poc_flag = (v >> 6) & 0x01;
	desc.picture_to_display_conversion_flag = (v >> 5) & 0x01;

	assert(0 == mpeg_bits_error(&bits));
	return descriptor_len+2;
}

size_t mpeg2_aac_descriptor(const uint8_t* data, size_t bytes)
{
	// 2.6.68 MPEG-2 AAC audio descriptor(p113)

	size_t descriptor_len;
	struct mpeg_bits_t bits;
	mpeg2_aac_descriptor_t desc;
	
	mpeg_bits_init(&bits, data, bytes);
	mpeg_bits_read8(&bits); // descriptor_tag
	descriptor_len = mpeg_bits_read8(&bits);
	assert(descriptor_len + 2 <= bytes);

	memset(&desc, 0, sizeof(desc));
	desc.profile = mpeg_bits_read8(&bits);
	desc.channel_configuration = mpeg_bits_read8(&bits);
	desc.additional_information = mpeg_bits_read8(&bits);

	assert(0 == mpeg_bits_error(&bits));
	return descriptor_len+2;
}

size_t svc_extension_descriptor(const uint8_t* data, size_t bytes)
{
	// 2.6.76 SVC extension descriptor(p116)

	uint8_t v;
	size_t descriptor_len;
	struct mpeg_bits_t bits;
	svc_extension_descriptor_t desc;

	mpeg_bits_init(&bits, data, bytes);
	mpeg_bits_read8(&bits); // descriptor_tag
	descriptor_len = mpeg_bits_read8(&bits);
	assert(descriptor_len + 2 <= bytes);

	memset(&desc, 0, sizeof(desc));
	desc.width = mpeg_bits_read16(&bits);
	desc.height = mpeg_bits_read16(&bits);
	desc.frame_rate = mpeg_bits_read16(&bits);
	desc.average_bitrate = mpeg_bits_read16(&bits);
	desc.maximum_bitrate = mpeg_bits_read16(&bits);
	desc.dependency_id = (mpeg_bits_read8(&bits) >> 5) & 0x07;
	v = mpeg_bits_read8(&bits);
	desc.quality_id_start = (v >> 4) & 0x0F;
	desc.quality_id_end = (v >> 0) & 0x0F;
	v = mpeg_bits_read8(&bits);
	desc.temporal_id_start = (v >> 5) & 0x07;
	desc.temporal_id_end = (v >> 2) & 0x07;
	desc.no_sei_nal_unit_present = (v >> 1) & 0x01;

	assert(0 == mpeg_bits_error(&bits));
	return descriptor_len+2;
}

size_t mvc_extension_descriptor(const uint8_t* data, size_t bytes)
{
	// 2.6.78 MVC extension descriptor(p117)

	uint32_t v;
	size_t descriptor_len;
	struct mpeg_bits_t bits;
	mvc_extension_descriptor_t desc;

	mpeg_bits_init(&bits, data, bytes);
	mpeg_bits_read8(&bits); // descriptor_tag
	descriptor_len = mpeg_bits_read8(&bits);
	assert(descriptor_len + 2 <= bytes);

	memset(&desc, 0, sizeof(desc));
	desc.average_bit_rate = mpeg_bits_read16(&bits);
	desc.maximum_bitrate = mpeg_bits_read16(&bits);
	v = mpeg_bits_read32(&bits);
	desc.view_order_index_min = (v >> 18) & 0x3FF;
	desc.view_order_index_max = (v >> 8) & 0x3FF;
	desc.temporal_id_start = (v >> 5) & 0x07;
	desc.temporal_id_end = (v >> 2) & 0x07;
	desc.no_sei_nal_unit_present = (v >> 1) & 0x01;
	desc.no_prefix_nal_unit_present = (v >> 0) & 0x01;

	assert(0 == mpeg_bits_error(&bits));
	return descriptor_len + 2;
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

size_t clock_extension_descriptor(const uint8_t* data, size_t bytes)
{
	uint32_t v;
	struct tm t;
	time_t clock;
	size_t descriptor_len;
	struct mpeg_bits_t bits;

	mpeg_bits_init(&bits, data, bytes);
	mpeg_bits_read8(&bits); // descriptor_tag
	descriptor_len = mpeg_bits_read8(&bits);
	assert(descriptor_len + 2 <= bytes);

	v = mpeg_bits_read32(&bits); // skip 4-bytes leading
	memset(&t, 0, sizeof(t));
	t.tm_year = mpeg_bits_read8(&bits) + 2000 - 1900;
	v = mpeg_bits_read32(&bits);
	t.tm_mon = ((v >> 28) & 0x0F) - 1;
	t.tm_mday = (v >> 23) & 0x1F;
	t.tm_hour = (v >> 18) & 0x1F;
	t.tm_min = (v >> 12) & 0x3F;
	t.tm_sec = (v >> 6) & 0x3F;
	//desc.microsecond = v & 0x3F;
	clock = mktime(&t) * 1000;

	assert(0 == mpeg_bits_error(&bits));
	return descriptor_len + 2;
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
