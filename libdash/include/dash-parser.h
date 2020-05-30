#ifndef _dash_parser_h_
#define _dash_parser_h_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum 
{
	DASH_STATIC = 0, 
	DASH_DYNAMIC, 
};

enum
{
	DASH_SEGMENT_NONE = 0,
	DASH_SEGMENT_BASE,
	DASH_SEGMENT_LIST,
	DASH_SEGMENT_TEMPLATE,
};

enum
{
	DASH_MEDIA_UNKNOWN,
	DASH_MEDIA_AUDIO,
	DASH_MEDIA_VIDEO,
	DASH_MEDIA_SUBTITLE,
};

enum { DASH_SCAN_UNKNOWN = 0, DASH_SCAN_PROGRESSIVE, DASH_SCAN_INTERLACED, };

struct dash_urltype_t
{
	char* source_url;
	char* range;
};

// BaseURL
struct dash_url_t
{
	size_t count;
	struct
	{
		char* uri;
		char* service_location;
		char* byte_range;
		double availability_time_offset;
		int availability_time_complete; // boolean
	} *urls;
};

struct dash_event_t
{
	uint64_t presentation_time;
	uint64_t duration;
	unsigned int id;
	char* message_data;
};

struct dash_event_stream_t
{
	char* href;
	char* actuate; // default: onRequest
	char* scheme_id_uri; // SchemeIdUri
	char* value;
	unsigned int timescale;

	size_t event_count;
	struct dash_event_t* events;
};

struct dash_label_t
{
	size_t count;
	struct
	{
		char* label;
		unsigned int id;
		char* lang;
	}*labels;
};

struct dash_program_information_t
{
	char* lang;
	char* more_information;

	char* title;
	char* source;
	char* copyright;
};

struct dash_descriptor_t
{
	size_t count;
	struct
	{
		char* scheme_uri;
		char* value;
		char* id;
	}* descs;
};

struct dash_metric_t
{
	char* metrics;

	size_t range_count;
	struct
	{
		double time;
		double duration;
	}* ranges;
	
	struct dash_descriptor_t reportings;
};

struct dash_segment_url_t
{
	char* media;
	char* media_range;
	char* index;
	char* index_range;
};

struct dash_segment_timeline_t
{
	size_t count;

	struct
	{
		uint64_t t;
		uint64_t n;
		uint64_t d;
		uint64_t k; // default 1
		int r; // default 0
	}* S;
};

struct dash_segment_t
{
	int type; // DASH_SEGMENT_BASE/DASH_SEGMENT_LIST/DASH_SEGMENT_TEMPLATE

	// segment base
	unsigned int timescale;
	uint64_t presentation_time_offset;
	uint64_t presentation_duration;
	double time_shift_buffer_depth;
	char* index_range;
	int index_range_exact; // 0-false, 1-true
	double availability_time_offset;
	int availability_time_complete;
	struct dash_urltype_t initialization; // Initialization
	struct dash_urltype_t representation_index; // RepresentationIndex
	
	// multiple segment base
	unsigned int duration;
	uint64_t start_number;
	struct dash_segment_timeline_t segment_timeline; // SegmentTimeline
	struct dash_urltype_t bitstream_switching; // BitstreamSwitching

	// segment list
	char* href;
	char* actuate; // default: onRequest
	size_t segment_url_count;
	struct dash_segment_url_t* segment_urls; // SegmentURL

	// segment template
	char* media;
	char* index;
	char* initialization_url; // initialization
	char* bitstream_switching_url;
};

struct dash_content_component_t
{
	unsigned int id;
	char* lang;
	char* content_type;
	char* par;
	char* tag;

	struct dash_descriptor_t accessibilities; // Accessibility
	struct dash_descriptor_t roles; // Role
	struct dash_descriptor_t ratings; // Rating
	struct dash_descriptor_t viewpoints; // Viewpoint
};

struct dash_representation_base_t
{
	char* profiles;
	unsigned int width;
	unsigned int height;
	char* sar;
	char* frame_rate;
	char* audio_sampling_rate;
	char* mime_type;
	char* segment_profiles;
	char* codecs;
	double maxmum_sap_period;
	char* start_with_sap;
	double max_playout_rate;
	int coding_dependency;
	int scan_type; // progressive, interlaced
	unsigned int selection_priority;
	int tag;

	struct dash_descriptor_t frame_packings; // FramePacking
	struct dash_descriptor_t audio_channel_configurations; // AudioChannelConfiguration
	struct dash_descriptor_t content_protections; // ContentProtection
	struct dash_descriptor_t essentials; // EssentialProperty
	struct dash_descriptor_t supplementals; // SupplementalProperty

	size_t inband_event_stream_count;
	struct dash_event_stream_t* inband_event_streams; // InbandEventStream

	size_t switching_count;
	struct
	{
		unsigned int interval;
		int type; // media, bitstream
	} *switchings; // Switching

	size_t random_access_count;
	struct
	{
		unsigned int interval;
		int type; // closed, open, gradual
		double min_buffer_time;
		unsigned int bandwidth;
	} *random_accesses; // RandomAccess

	struct dash_label_t group_labels; // GroupLabel
	struct dash_label_t labels; // Label
};

struct dash_preselection_t
{
	char* id;
	char* preselection_compoents;
	char* lang;

	struct dash_representation_base_t base;

	struct dash_descriptor_t accessibilities; // Accessibility
	struct dash_descriptor_t roles; // Role
	struct dash_descriptor_t ratings; // Rating
	struct dash_descriptor_t viewpoints; // Viewpoint
};

struct dash_subrepresentation_t
{
	unsigned int level;
	unsigned int dependency_level;
	unsigned int bandwidth;
	char* content_component;

	struct dash_representation_t* parent;
	struct dash_representation_base_t base;
};

struct dash_representation_t
{
	char* id;
	unsigned int bandwidth;
	unsigned int quality_ranking;
	char* dependncy_id;
	char* association_id;
	char* association_type;
	char* media_stream_structure_id;

	struct dash_adaptation_set_t* parent;
	struct dash_representation_base_t base;

	struct dash_url_t base_urls; // BaseURL

	size_t subrepresentation_count;
	struct dash_subrepresentation_t* subrepresentations; // SubRepresentation

	struct dash_segment_t segment;
};

struct dash_adaptation_set_t
{
	char* href;
	char* actuate; // default: onRequest
	unsigned int id;
	unsigned int group;
	char* lang;
	char* content_type; // text/image/audio/video/application/font
	char* par; // n:m
	unsigned int min_bandwidth;
	unsigned int max_bandwidth;
	unsigned int min_width;
	unsigned int max_width;
	unsigned int min_height;
	unsigned int max_height;
	char* min_framerate; // n/m
	char* max_framerate;
	int segment_alignment; // 0-false, 1-true
	int subsegment_aligment; // 0-false, 1-true
	int subsegment_start_with_sap; // default 0
	int bitstream_switching; // 0-false, 1-true

	struct dash_period_t* parent;
	struct dash_representation_base_t base;

	struct dash_descriptor_t accessibilities; // Accessibility
	struct dash_descriptor_t roles; // Role
	struct dash_descriptor_t ratings; // Rating
	struct dash_descriptor_t viewpoints; // Viewpoint

	size_t content_component_count;
	struct dash_content_component_t* content_components; // ContentComponent

	struct dash_url_t base_urls; // BaseURL

	struct dash_segment_t segment;

	size_t representation_count;
	struct dash_representation_t* representations; // Representation
};

struct dash_period_t
{
	char* href;
	char* actuate; // default: onRequest
	char* id;
	char* start; // start
	char* duration; // duration
	int bitstream_switching; // 0-false

	struct dash_mpd_t* parent;
	struct dash_url_t base_urls; // BaseURL

	struct dash_segment_t segment; // SegmentBase/SegmentList/SegmentTemplate

	struct dash_descriptor_t asset_identifier; // AssetIdentifier

	size_t event_stream_count;
	size_t event_stream_capacity;
	struct dash_event_stream_t* event_streams; // EventStream

	size_t adaptation_set_count;
	struct dash_adaptation_set_t* adaptation_sets; // AdaptationSet

	size_t subset_count;
	struct
	{
		char* contains;
		char* id;
	} *subsets; // SubSet

	struct dash_descriptor_t supplementals; // SupplementalProperty

	size_t empty_adaptation_set_count;
	struct dash_adaptation_set_t* empty_adaptation_sets; // EmptyAdaptationSet

	struct dash_label_t group_labels; // GroupLabel

	size_t preselection_count;
	struct dash_preselection_t* preselections; // Preselection
};

struct dash_mpd_t
{
	int type; // presentation type: 0-static, 1-dynamic
	char* id;
	char* profiles; // profiles
	char* availability_start_time;
	char* availability_end_time;
	char* publish_time;
	char* media_presentation_duration; // mediaPresentationDuration
	char* minimum_update_period; // minimumUpdatePeriod
	char* min_buffer_time; // minBufferTime
	char* time_shift_buffer_depth; // timeShiftBufferDepth
	char* suggested_presentation_delay; // suggestedPresentationDelay
	char* max_segment_duration; // maxSegmentDuration
	char* max_subsegment_duration; // maxSubsegmentDuration
	char* xsi; // xmlns:xsi
	char* xmlns; // xmlns
	char* schema_location; // xsi:schemaLocation

	size_t info_count;
	struct dash_program_information_t* infos; // ProgramInfomation

	struct dash_url_t urls; // BaseURL

	size_t location_count;
	char** locations; // Location

	size_t period_count;
	struct dash_period_t* periods; // Period

	size_t metric_count;
	struct dash_metric_t* metrics; // Metrics

	struct dash_descriptor_t essentials; // EssentialProperty
	struct dash_descriptor_t supplementals; // SupplementalProperty
	struct dash_descriptor_t timings; // UTCTming
};

/// Parse MPD(Media Presentation Description for MPEG-DASH ) manifest file
/// @param[out] mpd mpd object(free by dash_mpd_free)
/// @return 0-ok, other-error
int dash_mpd_parse(struct dash_mpd_t** mpd, const char* data, size_t bytes);

int dash_mpd_free(struct dash_mpd_t** mpd);

#ifdef __cplusplus
}
#endif
#endif /* !_dash_parser_h_ */
