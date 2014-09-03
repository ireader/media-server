#ifndef _mpeg_element_descriptor_h_
#define _mpeg_element_descriptor_h_

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
55-63	n/a n/a Rec. ITU-T H.222.0 | ISO/IEC 13818-1 Reserved
64-255	n/a n/a User Private
*/

typedef struct _video_stream_descriptor_t
{
	uint32_t multiple_frame_rate_flag : 1;
	// Table 2-47 ¨C Frame rate code
	// 23.976/24.0/25.0/29.97/30.0/50.0/59.94/60.0
	uint32_t frame_rate_code : 4; 
	uint32_t MPEG_1_only_flag : 1;
	uint32_t constrained_parameter_flag : 1;
	uint32_t still_picture_flag : 1;

	// MPEG_1_only_flag == 0
	uint32_t profile_and_level_indication : 8;
	uint32_t chroma_format : 2;
	uint32_t frame_rate_extension_flag : 1;
} video_stream_descriptor_t;

int video_stream_descriptor(const uint8_t* data, int bytes);

typedef struct _audio_stream_descriptor_t
{
	uint32_t free_format_flag : 1;
	uint32_t ID : 1;
	uint32_t layer : 2;
	uint32_t variable_rate_audio_indicator : 1;
} audio_stream_descriptor_t;

int audio_stream_descriptor(const uint8_t* data, int bytes);

/*
Table 2-50 ¨C Hierarchy_type field values
Value Description
0 Reserved
1 Spatial Scalability
2 SNR Scalability
3 Temporal Scalability
4 Data partitioning
5 Extension bitstream
6 Private Stream
7 Multi-view Profile
8 Combined Scalability
9 MVC video sub-bitstream
10-14 Reserved
15 Base layer or MVC base view sub-bitstream or AVC video sub-bitstream of MVC
*/
typedef struct _hierarchy_descriptor_t
{
	uint32_t reserved0 : 1;
	uint32_t temporal_scalability_flag : 1;
	uint32_t spatial_scalability_flag : 1;
	uint32_t quality_scalability_flag : 1;
	uint32_t hierarchy_type : 4;
	uint32_t tref_present_flag : 1;
	uint32_t reserved1 : 1;
	uint32_t hierarchy_layer_index : 6;
	uint32_t reserved2 : 2;
	uint32_t hierarchy_embedded_layer_index : 6;
	uint32_t reserved3 : 2;
	uint32_t hierarchy_channel : 6;
} hierarchy_descriptor_t;

int hierarchy_descriptor(const uint8_t* data, int bytes);

typedef struct _language_descriptor_t
{
	uint32_t code : 24;
	uint32_t audio : 8;
} language_descriptor_t;

int language_descriptor(const uint8_t* data, int bytes);

typedef struct _system_clock_descriptor_t
{
	uint32_t external_clock_reference_indicator : 1;
	uint32_t clock_accuracy_integer : 6;
	uint32_t clock_accuracy_exponent : 3;
} system_clock_descriptor_t;

int system_clock_descriptor(const uint8_t* data, int bytes);

typedef struct _mpeg4_video_descriptor_t
{
	uint8_t visual_profile_and_level;
} mpeg4_video_descriptor_t;

int mpeg4_video_descriptor(const uint8_t* data, int bytes);

typedef struct _mpeg4_audio_descriptor_t
{
	uint8_t profile_and_level;
} mpeg4_audio_descriptor_t;

int mpeg4_audio_descriptor(const uint8_t* data, int bytes);

typedef struct _avc_video_descriptor_t
{
	uint32_t profile_idc : 8;
	uint32_t constraint_set0_flag : 1;
	uint32_t constraint_set1_flag : 1;
	uint32_t constraint_set2_flag : 1;
	uint32_t constraint_set3_flag : 1;
	uint32_t constraint_set4_flag : 1;
	uint32_t constraint_set5_flag : 1;
	uint32_t AVC_compatible_flags : 2;
	uint32_t level_idc : 8;
	uint32_t AVC_still_present : 1;
	uint32_t AVC_24_hour_picture_flag : 1;
	uint32_t frame_packing_SEI_not_present_flag : 1;
} avc_video_descriptor_t;

int avc_video_descriptor(const uint8_t* data, int bytes);

typedef struct _avc_timing_hrd_descriptor_t
{
	uint32_t hrd_management_valid_flag : 1;
	uint32_t picture_and_timing_info_present : 1;
	uint32_t _90kHZ_flag : 1;
	uint32_t fixed_frame_rate_flag : 1;
	uint32_t temporal_poc_flag : 1;
	uint32_t picture_to_display_conversion_flag : 1;
	uint32_t N;
	uint32_t K;
	uint32_t num_unit_in_tick;
} avc_timing_hrd_descriptor_t;

int avc_timing_hrd_descriptor(const uint8_t* data, int bytes);

typedef struct _mpeg2_aac_descriptor_t
{
	uint32_t profile : 8;
	uint32_t channel_configuration : 8;
	uint32_t additional_information : 8;
} mpeg2_aac_descriptor_t;

int mpeg2_aac_descriptor(const uint8_t* data, int bytes);

typedef struct _mvc_extension_descriptor_t
{
	uint16_t average_bit_rate;
	uint16_t maximum_bitrate;
	uint32_t view_order_index_min : 10;
	uint32_t view_order_index_max : 10;
	uint32_t temporal_id_start : 3;
	uint32_t temporal_id_end : 3;
	uint32_t no_sei_nal_unit_present : 1;
	uint32_t no_prefix_nal_unit_present : 1;
} mvc_extension_descriptor_t;

int mvc_extension_descriptor(const uint8_t* data, int bytes);

#endif /* !_mpeg_element_descriptor_h_ */
