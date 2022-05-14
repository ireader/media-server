#ifndef _mpeg_element_descriptor_h_
#define _mpeg_element_descriptor_h_

#define SERVICE_ID		0x71
#define SERVICE_NAME	"ireader/media-server"

size_t mpeg_elment_descriptor(const uint8_t* data, size_t bytes);

typedef struct _video_stream_descriptor_t
{
	uint32_t multiple_frame_rate_flag : 1;
	// Table 2-47 - Frame rate code
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

size_t video_stream_descriptor(const uint8_t* data, size_t bytes);

typedef struct _audio_stream_descriptor_t
{
	uint32_t free_format_flag : 1;
	uint32_t ID : 1;
	uint32_t layer : 2;
	uint32_t variable_rate_audio_indicator : 1;
} audio_stream_descriptor_t;

size_t audio_stream_descriptor(const uint8_t* data, size_t bytes);

/*
Table 2-50 - Hierarchy_type field values
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
	uint32_t no_view_scalability_flag : 1;
	uint32_t no_temporal_scalability_flag : 1;
	uint32_t no_spatial_scalability_flag : 1;
	uint32_t no_quality_scalability_flag : 1;
	uint32_t hierarchy_type : 4;
	uint32_t tref_present_flag : 1;
	uint32_t reserved1 : 1;
	uint32_t hierarchy_layer_index : 6;
	uint32_t reserved2 : 2;
	uint32_t hierarchy_embedded_layer_index : 6;
	uint32_t reserved3 : 2;
	uint32_t hierarchy_channel : 6;
} hierarchy_descriptor_t;

size_t hierarchy_descriptor(const uint8_t* data, size_t bytes);

size_t registration_descriptor(const uint8_t* data, size_t bytes);

typedef struct _language_descriptor_t
{
	uint32_t code : 24;
	uint32_t audio : 8;
} language_descriptor_t;

size_t language_descriptor(const uint8_t* data, size_t bytes);

typedef struct _system_clock_descriptor_t
{
	uint32_t external_clock_reference_indicator : 1;
	uint32_t clock_accuracy_integer : 6;
	uint32_t clock_accuracy_exponent : 3;
} system_clock_descriptor_t;

size_t system_clock_descriptor(const uint8_t* data, size_t bytes);

typedef struct _mpeg4_video_descriptor_t
{
	uint8_t visual_profile_and_level;
} mpeg4_video_descriptor_t;

size_t mpeg4_video_descriptor(const uint8_t* data, size_t bytes);

typedef struct _metadata_pointer_descriptor
{
	uint32_t metadata_application_format_identifier;
	uint32_t metadata_format_identifier;
	uint8_t metadata_service_id;
	uint8_t metadata_locator_record_length;
	uint8_t MPEG_carriage_flags;
	uint16_t program_number;
	uint16_t transport_stream_location;
	uint16_t transport_stream_id;
} metadata_pointer_descriptor_t;

size_t metadata_pointer_descriptor(const uint8_t* data, size_t bytes);

typedef struct _metadata_descriptor_t
{
	uint32_t metadata_application_format_identifier;
	uint32_t metadata_format_identifier;
	uint8_t metadata_service_id;
	uint8_t service_identification_length;
	uint8_t decoder_config_flags;
	uint8_t decoder_config_length;
	uint8_t dec_config_identification_record_length;
	uint8_t decoder_config_metadata_service_id;
} metadata_descriptor_t;

size_t metadata_descriptor(const uint8_t* data, size_t bytes);

typedef struct _mpeg4_audio_descriptor_t
{
	uint8_t profile_and_level;
} mpeg4_audio_descriptor_t;

size_t mpeg4_audio_descriptor(const uint8_t* data, size_t bytes);

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

size_t avc_video_descriptor(const uint8_t* data, size_t bytes);

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

size_t avc_timing_hrd_descriptor(const uint8_t* data, size_t bytes);

typedef struct _mpeg2_aac_descriptor_t
{
	uint32_t profile : 8;
	uint32_t channel_configuration : 8;
	uint32_t additional_information : 8;
} mpeg2_aac_descriptor_t;

size_t mpeg2_aac_descriptor(const uint8_t* data, size_t bytes);

typedef struct _svc_extension_descriptor_t
{
	uint16_t width;
	uint16_t height;
	uint16_t frame_rate;
	uint16_t average_bitrate;
	uint16_t maximum_bitrate;
	uint32_t quality_id_start : 4;
	uint32_t quality_id_end : 4;
	uint32_t temporal_id_start : 3;
	uint32_t temporal_id_end : 3;
	uint32_t dependency_id : 3;
	uint32_t no_sei_nal_unit_present : 1;
} svc_extension_descriptor_t;

size_t svc_extension_descriptor(const uint8_t* data, size_t bytes);

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

size_t mvc_extension_descriptor(const uint8_t* data, size_t bytes);

size_t clock_extension_descriptor(const uint8_t* data, size_t bytes);
size_t clock_extension_descriptor_write(uint8_t* data, size_t bytes, int64_t clock);

size_t service_extension_descriptor_write(uint8_t* data, size_t bytes);

#endif /* !_mpeg_element_descriptor_h_ */
