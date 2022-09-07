// https://standards.iso.org/iso-iec/23009/-1/ed-3/en/DASH-MPD.xsd

#include "dash-parser.h"
#include "xs-datatype.h"
#include "hls-string.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifndef offsetof
#define offsetof(s, m)   (size_t)&(((s*)0)->m)
#endif

#define PRESELECTION_INCR 4
#define PRESELECTION_CAPACITY(count) ((count+PRESELECTION_INCR-1)/PRESELECTION_INCR*PRESELECTION_INCR)

#define ADAPTATION_SET_INCR 4
#define ADAPTATION_SET_CAPACITY(count) ((count+ADAPTATION_SET_INCR-1)/ADAPTATION_SET_INCR*ADAPTATION_SET_INCR)

#define REPRESENTATION_INCR 4
#define REPRESENTATION_CAPACITY(count) ((count+REPRESENTATION_INCR-1)/REPRESENTATION_INCR*REPRESENTATION_INCR)

#define SUBREPRESENTATION_INCR 4
#define SUBREPRESENTATION_CAPACITY(count) ((count+SUBREPRESENTATION_INCR-1)/SUBREPRESENTATION_INCR*SUBREPRESENTATION_INCR)

#define EVENT_CAPACITY_INCR 4
#define EVENT_CAPACITY(count) ((count+EVENT_CAPACITY_INCR-1)/EVENT_CAPACITY_INCR*EVENT_CAPACITY_INCR)

#define CONTENT_COMPONENT_CAPACITY_INCR 4
#define CONTENT_COMPONENT_CAPACITY(count) ((count+CONTENT_COMPONENT_CAPACITY_INCR-1)/CONTENT_COMPONENT_CAPACITY_INCR*CONTENT_COMPONENT_CAPACITY_INCR)

#define SUBSET_INCR 4
#define SUBSET_CAPACITY(count) ((count+SUBSET_INCR-1)/SUBSET_INCR*SUBSET_INCR)

#define SEGMENT_URL_INCR 8
#define SEGMENT_URL_CAPACITY(count) ((count+SEGMENT_URL_INCR-1)/SEGMENT_URL_INCR*SEGMENT_URL_INCR)

enum 
{ 
	DASH_TAG_ELEMENT, 
	DASH_TAG_DECLARATION, 
	DASH_TAG_COMMENT, 
	DASH_TAG_CDATA, 
};

enum
{
	DASH_TAG_FLAG_START		= 0x01, // start tag <section>
	DASH_TAG_FLAG_END		= 0x02, // 1-end tag </section>
	DASH_TAG_FLAG_LINBREAK	= 0x04, // empty-element tag <linebreak />
};

struct dash_tag_t
{
	int type;
	int flags;
	const char* ptr; // <
	const char* end; // >

	const char* name; // tag name
	size_t nlen; // tag name length in byte

	const char* attr;
	size_t nattr;
};

struct dash_parser_t
{
	size_t period_capacity;
	struct dash_mpd_t* mpd;
	char* content;
	void* tag; // dash tag object

	struct
	{
		const char* tag;
		void* ptr; // e.g. struct dash_mpd_t/struct dash_period_t/struct dash_adaptation_set_t
		size_t off;
	} stack[16];
	int level;
};

enum
{
	ATTR_VALUE_TYPE_UINT32,
	ATTR_VALUE_TYPE_UINT64,
	ATTR_VALUE_TYPE_FLOAT64,
	ATTR_VALUE_TYPE_STRING,
	ATTR_VALUE_TYPE_STRING_BOOL,
	ATTR_VALUE_TYPE_DATETIME,
	ATTR_VALUE_TYPE_DURATION,
	//ATTR_VALUE_TYPE_FRAME_RATE,
	//ATTR_VALUE_TYPE_RATIO,
};

struct dash_tag_attr_t
{
	int cls;
	const char* name;
	void* ptr;
};

#define DASH_TAG_ATTR_VALUE(a, cls0, name0, ptr0) {a.cls=cls0; a.name=name0; a.ptr=ptr0; }

static int dash_attr_read(const char* value, size_t n, int cls, void* ptr)
{
	switch (cls)
	{
	case ATTR_VALUE_TYPE_STRING_BOOL:
		*(int*)ptr = (4 == n && 0 == strncasecmp(value, "true", 4)) ? 1 : 0;
		return 0;

	case ATTR_VALUE_TYPE_UINT32:
		*(uint32_t*)ptr = (uint32_t)strtoul(value, NULL, 10);
		break;

	case ATTR_VALUE_TYPE_UINT64:
		*(uint64_t*)ptr = (uint64_t)strtoull(value, NULL, 10);
		break;

	case ATTR_VALUE_TYPE_FLOAT64:
		*(double*)ptr = strtod(value, NULL);
		break;

	case ATTR_VALUE_TYPE_STRING:
		*((char**)ptr) = (char*)value;
		((char*)value)[n] = 0;
		break;

	case ATTR_VALUE_TYPE_DATETIME:
		*((char**)ptr) = (char*)value;
		((char*)value)[n] = 0;
		break;

	case ATTR_VALUE_TYPE_DURATION:
		return xs_duration_read((int64_t*)ptr, value, (int)n);

	//case ATTR_VALUE_TYPE_FRAME_RATE:
	//	// [0-9]*[0-9](/[0-9]*[0-9])?
	//	break;

	//case ATTR_VALUE_TYPE_RATIO:
	//	// [0-9]*:[0-9]*
	//	sscanf(value, "%d:%d", &num, &den);
	//	break;

	default:
		assert(0);
		return -1;
	}

	return 0;
}

static int dash_parse_attrs(const char* data, size_t bytes, struct dash_tag_attr_t* attrs, size_t nattrs)
{
	int r;
	size_t i, n, nn, nv;
	const char* ptr, * next;
	const char* name, * value;

	r = 0;
	for (ptr = data; ptr && ptr < data + bytes && 0 == r; ptr = next)
	{
		n = hls_strsplit(ptr, data + bytes, " \r\n", "\"", &next);

		nn = hls_strsplit(ptr, ptr + n, "=", "", &value);
		name = hls_strtrim(ptr, &nn, " \t\r\n", " \t\r\n"); // trim SP/HTAB
		nv = ptr + n - value;
		value = hls_strtrim(value, &nv, " \t\r\n'\"", " \t\r\n'\""); // trim SP/HTAB/'/"

		for (i = 0; i < nattrs; i++)
		{
			if (nn == strlen(attrs[i].name) && 0 == strncasecmp(attrs[i].name, name, nn))
			{
				r = dash_attr_read(value, nv, attrs[i].cls, attrs[i].ptr);
				break;
			}
		}
	}

	return r;
}

static int dash_parser_realloc(void** ptr, size_t* capacity, size_t len, size_t incr, size_t size)
{
	void* ptr1;
	if (len >= *capacity)
	{
		assert(incr > 0);
		ptr1 = realloc(*ptr, (len + incr) * size);
		if (!ptr1)
			return -ENOMEM;

		memset((uint8_t*)ptr1 + len * size, 0, incr * size);
		*capacity = len + incr;
		*ptr = ptr1;
	}
	return 0;
}

static int dash_tag_mpd(struct dash_parser_t* parser, void* ptr, const char* attr, size_t len)
{
	int r;
	struct dash_tag_attr_t attrs[16];
	struct dash_mpd_t* mpd;
	char* type;
	
	(void)ptr;
	mpd = parser->mpd;
	parser->tag = mpd; // save
	type = NULL;

	DASH_TAG_ATTR_VALUE(attrs[0], ATTR_VALUE_TYPE_STRING, "id", &mpd->id);
	DASH_TAG_ATTR_VALUE(attrs[1], ATTR_VALUE_TYPE_STRING, "profiles", &mpd->profiles);
	DASH_TAG_ATTR_VALUE(attrs[2], ATTR_VALUE_TYPE_STRING, "type", &type);
	DASH_TAG_ATTR_VALUE(attrs[3], ATTR_VALUE_TYPE_DATETIME, "availabilityStartTime", &mpd->availability_start_time);
	DASH_TAG_ATTR_VALUE(attrs[4], ATTR_VALUE_TYPE_DATETIME, "availabilityEndTime", &mpd->availability_end_time);
	DASH_TAG_ATTR_VALUE(attrs[5], ATTR_VALUE_TYPE_DATETIME, "publishTime", &mpd->publish_time);
	DASH_TAG_ATTR_VALUE(attrs[6], ATTR_VALUE_TYPE_DURATION, "mediaPresentationDuration", &mpd->media_presentation_duration);
	DASH_TAG_ATTR_VALUE(attrs[7], ATTR_VALUE_TYPE_DURATION, "minimumUpdatePeriod", &mpd->minimum_update_period);
	DASH_TAG_ATTR_VALUE(attrs[8], ATTR_VALUE_TYPE_DURATION, "minBufferTime", &mpd->min_buffer_time);
	DASH_TAG_ATTR_VALUE(attrs[9], ATTR_VALUE_TYPE_DURATION, "timeShiftBufferDepath", &mpd->time_shift_buffer_depth);
	DASH_TAG_ATTR_VALUE(attrs[10], ATTR_VALUE_TYPE_DURATION, "suggestedPresentationDelay", &mpd->suggested_presentation_delay);
	DASH_TAG_ATTR_VALUE(attrs[11], ATTR_VALUE_TYPE_DURATION, "maxSegmentDuration", &mpd->max_segment_duration);
	DASH_TAG_ATTR_VALUE(attrs[12], ATTR_VALUE_TYPE_DURATION, "maxSubsegmentDuration", &mpd->max_subsegment_duration);
	DASH_TAG_ATTR_VALUE(attrs[13], ATTR_VALUE_TYPE_STRING, "xmlns:xsi", &mpd->xsi);
	DASH_TAG_ATTR_VALUE(attrs[14], ATTR_VALUE_TYPE_STRING, "xmlns", &mpd->xmlns);
	DASH_TAG_ATTR_VALUE(attrs[15], ATTR_VALUE_TYPE_STRING, "xsi:schemaLocation", &mpd->schema_location);

	r = dash_parse_attrs(attr, len, attrs, sizeof(attrs) / sizeof(attrs[0]));
	if (0 != r)
		return r;

	if (type && 0 == strcasecmp(type, "dynamic"))
		mpd->type = DASH_DYNAMIC;
	else
		mpd->type = DASH_STATIC;
	return 0;
}

static int dash_tag_period(struct dash_parser_t* parser, void* ptr, const char* attr, size_t len)
{
	int r;
	struct dash_tag_attr_t attrs[6];
	struct dash_period_t* period;
	struct dash_mpd_t* mpd;

	mpd = (struct dash_mpd_t*)ptr;
	if (0 != dash_parser_realloc((void**)&mpd->periods, &parser->period_capacity, mpd->period_count, 2, sizeof(mpd->periods[0])))
		return -ENOMEM;

	period = &mpd->periods[mpd->period_count];
	period->parent = mpd;
	period->actuate = "onRequest"; // default
	DASH_TAG_ATTR_VALUE(attrs[0], ATTR_VALUE_TYPE_STRING, "id", &period->id);
	DASH_TAG_ATTR_VALUE(attrs[1], ATTR_VALUE_TYPE_STRING, "xlink:href", &period->href);
	DASH_TAG_ATTR_VALUE(attrs[2], ATTR_VALUE_TYPE_STRING, "xlink:actuate", &period->actuate);
	DASH_TAG_ATTR_VALUE(attrs[3], ATTR_VALUE_TYPE_DURATION, "start", &period->start);
	DASH_TAG_ATTR_VALUE(attrs[4], ATTR_VALUE_TYPE_DURATION, "duration", &period->duration);
	DASH_TAG_ATTR_VALUE(attrs[5], ATTR_VALUE_TYPE_STRING_BOOL, "bitstreamSwitching", &period->bitstream_switching);

	r = dash_parse_attrs(attr, len, attrs, sizeof(attrs) / sizeof(attrs[0]));
	if (0 != r)
		return r;

	parser->tag = period; // save
	mpd->period_count++;
	return 0;
}

static int dash_tag_representation_base(struct dash_representation_base_t* base, const char* attr, size_t len)
{
	struct dash_tag_attr_t attrs[16];
	base->selection_priority = 1; // default 1
	DASH_TAG_ATTR_VALUE(attrs[0], ATTR_VALUE_TYPE_STRING, "profiles", &base->profiles);
	DASH_TAG_ATTR_VALUE(attrs[1], ATTR_VALUE_TYPE_UINT32, "width", &base->width);
	DASH_TAG_ATTR_VALUE(attrs[2], ATTR_VALUE_TYPE_UINT32, "height", &base->height);
	DASH_TAG_ATTR_VALUE(attrs[3], ATTR_VALUE_TYPE_STRING, "sar", &base->sar);
	DASH_TAG_ATTR_VALUE(attrs[4], ATTR_VALUE_TYPE_STRING, "frameRate", &base->frame_rate);
	DASH_TAG_ATTR_VALUE(attrs[5], ATTR_VALUE_TYPE_STRING, "audioSamplingRate", &base->audio_sampling_rate);
	DASH_TAG_ATTR_VALUE(attrs[6], ATTR_VALUE_TYPE_STRING, "mimeType", &base->mime_type);
	DASH_TAG_ATTR_VALUE(attrs[7], ATTR_VALUE_TYPE_STRING, "segmentProfiles", &base->segment_profiles);
	DASH_TAG_ATTR_VALUE(attrs[8], ATTR_VALUE_TYPE_STRING, "codecs", &base->codecs);
	DASH_TAG_ATTR_VALUE(attrs[9], ATTR_VALUE_TYPE_FLOAT64, "maximumSAPPeriod", &base->maxmum_sap_period);
	DASH_TAG_ATTR_VALUE(attrs[10], ATTR_VALUE_TYPE_STRING, "startWithSAP", &base->start_with_sap);
	DASH_TAG_ATTR_VALUE(attrs[11], ATTR_VALUE_TYPE_FLOAT64, "maxPlayoutRate", &base->max_playout_rate);
	DASH_TAG_ATTR_VALUE(attrs[12], ATTR_VALUE_TYPE_STRING_BOOL, "codingDependency", &base->coding_dependency);
	DASH_TAG_ATTR_VALUE(attrs[13], ATTR_VALUE_TYPE_STRING, "scanType", &base->scan_type);
	DASH_TAG_ATTR_VALUE(attrs[14], ATTR_VALUE_TYPE_UINT32, "selectionPriority", &base->selection_priority);
	DASH_TAG_ATTR_VALUE(attrs[15], ATTR_VALUE_TYPE_STRING, "tag", &base->tag);
	return dash_parse_attrs(attr, len, attrs, sizeof(attrs) / sizeof(attrs[0]));
}

static int dash_tag_preselection(struct dash_parser_t* parser, void* ptr, const char* attr, size_t len)
{
	int r;
	size_t capacity;
	struct dash_tag_attr_t attrs[3];
	struct dash_period_t* period;
	struct dash_preselection_t* preselection;

	period = (struct dash_period_t*)ptr;
	capacity = PRESELECTION_CAPACITY(period->preselection_count);
	if (0 != dash_parser_realloc((void**)&period->preselections, &capacity, period->preselection_count, PRESELECTION_INCR, sizeof(period->preselections[0])))
		return -ENOMEM;

	preselection = &period->preselections[period->preselection_count];
	DASH_TAG_ATTR_VALUE(attrs[0], ATTR_VALUE_TYPE_STRING, "id", &preselection->id);
	DASH_TAG_ATTR_VALUE(attrs[1], ATTR_VALUE_TYPE_STRING, "preselectionComponents", &preselection->preselection_compoents);
	DASH_TAG_ATTR_VALUE(attrs[2], ATTR_VALUE_TYPE_STRING, "lang", &preselection->lang);

	r = dash_parse_attrs(attr, len, attrs, sizeof(attrs) / sizeof(attrs[0]));
	if (0 == r)
		r = dash_tag_representation_base(&preselection->base, attr, len);
	if (0 != r)
		return r;

	parser->tag = preselection; // save
	period->preselection_count++;
	return 0;
}

static int dash_tag_adaptation_set(struct dash_parser_t* parser, void* ptr, const char* attr, size_t len)
{
	int r;
	size_t capacity;
	struct dash_tag_attr_t attrs[19];
	struct dash_period_t* period;
	struct dash_adaptation_set_t* adaptation_set;

	period = (struct dash_period_t*)ptr;
	capacity = ADAPTATION_SET_CAPACITY(period->adaptation_set_count);
	if (0 != dash_parser_realloc((void**)&period->adaptation_sets, &capacity, period->adaptation_set_count, ADAPTATION_SET_INCR, sizeof(period->adaptation_sets[0])))
		return -ENOMEM;

	adaptation_set = &period->adaptation_sets[period->adaptation_set_count];
	adaptation_set->parent = period;
	adaptation_set->actuate = "onRequest"; // default
	adaptation_set->segment_alignment = 0; // default
	adaptation_set->subsegment_aligment = 0; // default
	adaptation_set->subsegment_start_with_sap = 0; // default
	DASH_TAG_ATTR_VALUE(attrs[0], ATTR_VALUE_TYPE_STRING, "xlink:href", &adaptation_set->href);
	DASH_TAG_ATTR_VALUE(attrs[1], ATTR_VALUE_TYPE_STRING, "xlink:actuate", &adaptation_set->actuate);
	DASH_TAG_ATTR_VALUE(attrs[2], ATTR_VALUE_TYPE_UINT32, "id", &adaptation_set->id);
	DASH_TAG_ATTR_VALUE(attrs[3], ATTR_VALUE_TYPE_UINT32, "group", &adaptation_set->group);
	DASH_TAG_ATTR_VALUE(attrs[4], ATTR_VALUE_TYPE_STRING, "lang", &adaptation_set->lang);
	DASH_TAG_ATTR_VALUE(attrs[5], ATTR_VALUE_TYPE_STRING, "contentType", &adaptation_set->content_type);
	DASH_TAG_ATTR_VALUE(attrs[6], ATTR_VALUE_TYPE_STRING, "par", &adaptation_set->par);
	DASH_TAG_ATTR_VALUE(attrs[7], ATTR_VALUE_TYPE_UINT32, "minBandwidth", &adaptation_set->min_bandwidth);
	DASH_TAG_ATTR_VALUE(attrs[8], ATTR_VALUE_TYPE_UINT32, "maxBandwidth", &adaptation_set->max_bandwidth);
	DASH_TAG_ATTR_VALUE(attrs[9], ATTR_VALUE_TYPE_UINT32, "minWidth", &adaptation_set->min_width);
	DASH_TAG_ATTR_VALUE(attrs[10], ATTR_VALUE_TYPE_UINT32, "maxWidth", &adaptation_set->max_width);
	DASH_TAG_ATTR_VALUE(attrs[11], ATTR_VALUE_TYPE_UINT32, "minHeight", &adaptation_set->min_height);
	DASH_TAG_ATTR_VALUE(attrs[12], ATTR_VALUE_TYPE_UINT32, "maxHeight", &adaptation_set->max_height);
	DASH_TAG_ATTR_VALUE(attrs[13], ATTR_VALUE_TYPE_STRING, "minFrameRate", &adaptation_set->min_framerate);
	DASH_TAG_ATTR_VALUE(attrs[14], ATTR_VALUE_TYPE_STRING, "maxFrameRate", &adaptation_set->max_framerate);
	DASH_TAG_ATTR_VALUE(attrs[15], ATTR_VALUE_TYPE_STRING_BOOL, "segmentAlignment", &adaptation_set->segment_alignment);
	DASH_TAG_ATTR_VALUE(attrs[16], ATTR_VALUE_TYPE_STRING_BOOL, "subsegmentAlignment", &adaptation_set->subsegment_aligment);
	DASH_TAG_ATTR_VALUE(attrs[17], ATTR_VALUE_TYPE_UINT32, "subsegmentStartsWithSAP", &adaptation_set->subsegment_start_with_sap);
	DASH_TAG_ATTR_VALUE(attrs[18], ATTR_VALUE_TYPE_STRING_BOOL, "bitstreamSwitching", &adaptation_set->bitstream_switching);

	r = dash_parse_attrs(attr, len, attrs, sizeof(attrs) / sizeof(attrs[0]));
	if (0 == r)
		r = dash_tag_representation_base(&adaptation_set->base, attr, len);
	if (0 != r)
		return r;

	parser->tag = adaptation_set; // save
	period->adaptation_set_count++;
	return 0;
}

static int dash_tag_empty_adaptation_set(struct dash_parser_t* parser, void* ptr, const char* attr, size_t len)
{
	int r;
	size_t capacity;
	struct dash_tag_attr_t attrs[19];
	struct dash_period_t* period;
	struct dash_adaptation_set_t* adaptation_set;

	period = (struct dash_period_t*)ptr;
	capacity = ADAPTATION_SET_CAPACITY(period->empty_adaptation_set_count);
	if (0 != dash_parser_realloc((void**)&period->empty_adaptation_sets, &capacity, period->empty_adaptation_set_count, ADAPTATION_SET_INCR, sizeof(period->empty_adaptation_sets[0])))
		return -ENOMEM;

	adaptation_set = &period->empty_adaptation_sets[period->empty_adaptation_set_count];
	adaptation_set->actuate = "onRequest"; // default
	DASH_TAG_ATTR_VALUE(attrs[0], ATTR_VALUE_TYPE_STRING, "xlink:href", &adaptation_set->href);
	DASH_TAG_ATTR_VALUE(attrs[1], ATTR_VALUE_TYPE_STRING, "xlink:actuate", &adaptation_set->actuate);
	DASH_TAG_ATTR_VALUE(attrs[2], ATTR_VALUE_TYPE_UINT32, "id", &adaptation_set->id);
	DASH_TAG_ATTR_VALUE(attrs[3], ATTR_VALUE_TYPE_UINT32, "group", &adaptation_set->group);
	DASH_TAG_ATTR_VALUE(attrs[4], ATTR_VALUE_TYPE_STRING, "lang", &adaptation_set->lang);
	DASH_TAG_ATTR_VALUE(attrs[5], ATTR_VALUE_TYPE_STRING, "contentType", &adaptation_set->content_type);
	DASH_TAG_ATTR_VALUE(attrs[6], ATTR_VALUE_TYPE_STRING, "par", &adaptation_set->par);
	DASH_TAG_ATTR_VALUE(attrs[7], ATTR_VALUE_TYPE_UINT32, "minBandwidth", &adaptation_set->min_bandwidth);
	DASH_TAG_ATTR_VALUE(attrs[8], ATTR_VALUE_TYPE_UINT32, "maxBandwidth", &adaptation_set->max_bandwidth);
	DASH_TAG_ATTR_VALUE(attrs[9], ATTR_VALUE_TYPE_UINT32, "minWidth", &adaptation_set->min_width);
	DASH_TAG_ATTR_VALUE(attrs[10], ATTR_VALUE_TYPE_UINT32, "maxWidth", &adaptation_set->max_width);
	DASH_TAG_ATTR_VALUE(attrs[11], ATTR_VALUE_TYPE_UINT32, "minHeight", &adaptation_set->min_height);
	DASH_TAG_ATTR_VALUE(attrs[12], ATTR_VALUE_TYPE_UINT32, "maxHeight", &adaptation_set->max_height);
	DASH_TAG_ATTR_VALUE(attrs[13], ATTR_VALUE_TYPE_STRING, "minFrameRate", &adaptation_set->min_framerate);
	DASH_TAG_ATTR_VALUE(attrs[14], ATTR_VALUE_TYPE_STRING, "maxFrameRate", &adaptation_set->max_framerate);
	DASH_TAG_ATTR_VALUE(attrs[15], ATTR_VALUE_TYPE_STRING_BOOL, "segmentAlignment", &adaptation_set->segment_alignment);
	DASH_TAG_ATTR_VALUE(attrs[16], ATTR_VALUE_TYPE_STRING_BOOL, "sugsegmentAlignment", &adaptation_set->subsegment_aligment);
	DASH_TAG_ATTR_VALUE(attrs[17], ATTR_VALUE_TYPE_STRING, "subsegmentStartsWithSAP", &adaptation_set->subsegment_start_with_sap);
	DASH_TAG_ATTR_VALUE(attrs[18], ATTR_VALUE_TYPE_STRING_BOOL, "bitstreamSwitching", &adaptation_set->bitstream_switching);

	r = dash_parse_attrs(attr, len, attrs, sizeof(attrs) / sizeof(attrs[0]));
	if (0 == r)
		r = dash_tag_representation_base(&adaptation_set->base, attr, len);
	if (0 != r)
		return r;

	parser->tag = adaptation_set; // save
	period->empty_adaptation_set_count++;
	return 0;
}

static int dash_tag_representation(struct dash_parser_t* parser, void* ptr, const char* attr, size_t len)
{
	int r;
	size_t capacity;
	struct dash_tag_attr_t attrs[7];
	struct dash_adaptation_set_t* adaptation_set;
	struct dash_representation_t* representation;

	adaptation_set = (struct dash_adaptation_set_t*)ptr;
	capacity = REPRESENTATION_CAPACITY(adaptation_set->representation_count);
	if (0 != dash_parser_realloc((void**)&adaptation_set->representations, &capacity, adaptation_set->representation_count, REPRESENTATION_INCR, sizeof(adaptation_set->representations[0])))
		return -ENOMEM;

	representation = &adaptation_set->representations[adaptation_set->representation_count];
	representation->parent = adaptation_set;

	DASH_TAG_ATTR_VALUE(attrs[0], ATTR_VALUE_TYPE_STRING, "id", &representation->id);
	DASH_TAG_ATTR_VALUE(attrs[1], ATTR_VALUE_TYPE_UINT32, "bandwidth", &representation->bandwidth);
	DASH_TAG_ATTR_VALUE(attrs[2], ATTR_VALUE_TYPE_UINT32, "qualityRanking", &representation->quality_ranking);
	DASH_TAG_ATTR_VALUE(attrs[3], ATTR_VALUE_TYPE_STRING, "dependencyId", &representation->dependncy_id);
	DASH_TAG_ATTR_VALUE(attrs[4], ATTR_VALUE_TYPE_STRING, "associationId", &representation->association_id);
	DASH_TAG_ATTR_VALUE(attrs[5], ATTR_VALUE_TYPE_STRING, "associationType", &representation->association_type);
	DASH_TAG_ATTR_VALUE(attrs[6], ATTR_VALUE_TYPE_STRING, "mediaStreamStructureId", &representation->media_stream_structure_id);

	r = dash_parse_attrs(attr, len, attrs, sizeof(attrs) / sizeof(attrs[0]));
	if (0 == r)
		r = dash_tag_representation_base(&representation->base, attr, len);
	if (0 != r)
		return r;

	parser->tag = representation; // save
	adaptation_set->representation_count++;
	return 0;
}

static int dash_tag_subrepresentation(struct dash_parser_t* parser, void* ptr, const char* attr, size_t len)
{
	int r;
	size_t capacity;
	struct dash_tag_attr_t attrs[4];
	struct dash_representation_t* representation;
	struct dash_subrepresentation_t* subrepresentation;

	representation = (struct dash_representation_t*)ptr;
	capacity = SUBREPRESENTATION_CAPACITY(representation->subrepresentation_count);
	if (0 != dash_parser_realloc((void**)&representation->subrepresentations, &capacity, representation->subrepresentation_count, SUBREPRESENTATION_INCR, sizeof(representation->subrepresentations[0])))
		return -ENOMEM;

	subrepresentation = &representation->subrepresentations[representation->subrepresentation_count];
	subrepresentation->parent = representation;

	DASH_TAG_ATTR_VALUE(attrs[0], ATTR_VALUE_TYPE_UINT32, "level", &subrepresentation->level);
	DASH_TAG_ATTR_VALUE(attrs[1], ATTR_VALUE_TYPE_STRING, "dependencyLevel", &subrepresentation->dependency_level);
	DASH_TAG_ATTR_VALUE(attrs[2], ATTR_VALUE_TYPE_UINT32, "bandwidth", &subrepresentation->bandwidth);
	DASH_TAG_ATTR_VALUE(attrs[3], ATTR_VALUE_TYPE_STRING, "contentComponent", &subrepresentation->content_component);

	r = dash_parse_attrs(attr, len, attrs, sizeof(attrs) / sizeof(attrs[0]));
	if (0 == r)
		r = dash_tag_representation_base(&subrepresentation->base, attr, len);
	if (0 != r)
		return r;

	parser->tag = subrepresentation; // save
	representation->subrepresentation_count++;
	return 0;
}

static int dash_tag_segment(struct dash_parser_t* parser, void* ptr, int type, const char* attr, size_t len)
{
	//const char* tags[] = { "SegmentBase", "SegmentList", "SegmentTemplate" };
	struct dash_tag_attr_t attrs[16];
	struct dash_segment_t* segment;

	assert(type > 0 && type <= 3);
	segment = (struct dash_segment_t*)ptr;
	segment->type = type;
	segment->actuate = "onRequest";
	DASH_TAG_ATTR_VALUE(attrs[0], ATTR_VALUE_TYPE_UINT32, "timescale", &segment->timescale);
	DASH_TAG_ATTR_VALUE(attrs[1], ATTR_VALUE_TYPE_UINT64, "presentationTimeOffset", &segment->presentation_time_offset);
	DASH_TAG_ATTR_VALUE(attrs[2], ATTR_VALUE_TYPE_UINT64, "presentationDuration", &segment->presentation_duration);
	DASH_TAG_ATTR_VALUE(attrs[3], ATTR_VALUE_TYPE_DURATION, "timeShiftBufferDepath", &segment->time_shift_buffer_depth);
	DASH_TAG_ATTR_VALUE(attrs[4], ATTR_VALUE_TYPE_STRING, "indexRange", &segment->index_range);
	DASH_TAG_ATTR_VALUE(attrs[5], ATTR_VALUE_TYPE_STRING_BOOL, "indexRangeExact", &segment->index_range_exact);
	DASH_TAG_ATTR_VALUE(attrs[6], ATTR_VALUE_TYPE_FLOAT64, "availabiltiyTimeOffset", &segment->availability_time_offset);
	DASH_TAG_ATTR_VALUE(attrs[7], ATTR_VALUE_TYPE_STRING_BOOL, "availabilityTimeComplete", &segment->availability_time_complete);
	// MultipleSegmentBaseType
	DASH_TAG_ATTR_VALUE(attrs[8], ATTR_VALUE_TYPE_UINT32, "duration", &segment->duration);
	DASH_TAG_ATTR_VALUE(attrs[9], ATTR_VALUE_TYPE_UINT64, "startNumber", &segment->start_number);
	// SegmentListType
	DASH_TAG_ATTR_VALUE(attrs[10], ATTR_VALUE_TYPE_STRING, "xlink:href", &segment->href);
	DASH_TAG_ATTR_VALUE(attrs[11], ATTR_VALUE_TYPE_STRING, "xlink:actuate", &segment->actuate);
	// SegmentTemplateType
	DASH_TAG_ATTR_VALUE(attrs[12], ATTR_VALUE_TYPE_STRING, "media", &segment->media);
	DASH_TAG_ATTR_VALUE(attrs[13], ATTR_VALUE_TYPE_STRING, "index", &segment->index);
	DASH_TAG_ATTR_VALUE(attrs[14], ATTR_VALUE_TYPE_STRING, "initialization", &segment->initialization_url);
	DASH_TAG_ATTR_VALUE(attrs[15], ATTR_VALUE_TYPE_STRING, "bitstreamSwitching", &segment->bitstream_switching_url);

	parser->tag = segment; // save
	return dash_parse_attrs(attr, len, attrs, sizeof(attrs) / sizeof(attrs[0]));
}

static int dash_tag_segment_base(struct dash_parser_t* parser, void* ptr, const char* attr, size_t len)
{
	return dash_tag_segment(parser, ptr, DASH_SEGMENT_BASE, attr, len);
}

static int dash_tag_segment_list(struct dash_parser_t* parser, void* ptr, const char* attr, size_t len)
{
	return dash_tag_segment(parser, ptr, DASH_SEGMENT_LIST, attr, len);
}

static int dash_tag_segment_template(struct dash_parser_t* parser, void* ptr, const char* attr, size_t len)
{
	return dash_tag_segment(parser, ptr, DASH_SEGMENT_TEMPLATE, attr, len);
}

static int dash_tag_segment_timeline(struct dash_parser_t* parser, void* ptr, const char* attr, size_t len)
{
	struct dash_segment_t* segment;
	struct dash_segment_timeline_t* timeline;
	segment = (struct dash_segment_t*)ptr;
	timeline = &segment->segment_timeline;
	parser->tag = timeline; // save
	(void)attr, (void)len;
	return 0;
}

static int dash_tag_s(struct dash_parser_t* parser, void* ptr, const char* attr, size_t len)
{
	int r;
	size_t capacity;
	struct dash_tag_attr_t attrs[5];
	struct dash_segment_timeline_t* timeline;

	timeline = (struct dash_segment_timeline_t*)ptr;
	capacity = SEGMENT_URL_CAPACITY(timeline->count);
	if (0 != dash_parser_realloc((void**)&timeline->S, &capacity, timeline->count, SEGMENT_URL_INCR, sizeof(timeline->S[0])))
		return -ENOMEM;

	timeline->S[timeline->count].k = 1;
	DASH_TAG_ATTR_VALUE(attrs[0], ATTR_VALUE_TYPE_UINT64, "t", &timeline->S[timeline->count].t);
	DASH_TAG_ATTR_VALUE(attrs[1], ATTR_VALUE_TYPE_UINT64, "n", &timeline->S[timeline->count].n);
	DASH_TAG_ATTR_VALUE(attrs[2], ATTR_VALUE_TYPE_UINT64, "d", &timeline->S[timeline->count].d);
	DASH_TAG_ATTR_VALUE(attrs[3], ATTR_VALUE_TYPE_UINT64, "k", &timeline->S[timeline->count].k);
	DASH_TAG_ATTR_VALUE(attrs[4], ATTR_VALUE_TYPE_UINT32, "r", &timeline->S[timeline->count].r);

	r = dash_parse_attrs(attr, len, attrs, sizeof(attrs) / sizeof(attrs[0]));
	if (0 != r)
		return r;

	parser->tag = &timeline->S[timeline->count]; // save
	timeline->count++;
	return 0;
}

static int dash_tag_segment_url(struct dash_parser_t* parser, void* ptr, const char* attr, size_t len)
{
	int r;
	size_t capacity;
	struct dash_tag_attr_t attrs[4];
	struct dash_segment_url_t* url;
	struct dash_segment_t* segment;

	segment = (struct dash_segment_t*)ptr;
	capacity = SEGMENT_URL_CAPACITY(segment->segment_url_count);
	if (0 != dash_parser_realloc((void**)&segment->segment_urls, &capacity, segment->segment_url_count, SEGMENT_URL_INCR, sizeof(segment->segment_urls[0])))
		return -ENOMEM;

	url = &segment->segment_urls[segment->segment_url_count];
	DASH_TAG_ATTR_VALUE(attrs[0], ATTR_VALUE_TYPE_STRING, "media", &url->media);
	DASH_TAG_ATTR_VALUE(attrs[1], ATTR_VALUE_TYPE_STRING, "mediaRange", &url->media_range);
	DASH_TAG_ATTR_VALUE(attrs[2], ATTR_VALUE_TYPE_DURATION, "index", &url->index);
	DASH_TAG_ATTR_VALUE(attrs[3], ATTR_VALUE_TYPE_UINT32, "indexRange", &url->index_range);

	r = dash_parse_attrs(attr, len, attrs, sizeof(attrs) / sizeof(attrs[0]));
	if (0 != r)
		return r;

	parser->tag = url; // save
	segment->segment_url_count++;
	return 0;
}

static int dash_tag_baseurl(struct dash_parser_t* parser, void* ptr, const char* attr, size_t len)
{
	size_t capacity;
	struct dash_tag_attr_t attrs[4];
	struct dash_url_t* url;

	url = (struct dash_url_t*)ptr;
	capacity = url->count;
	if (0 != dash_parser_realloc((void**)&url->urls, &capacity, url->count, 1, sizeof(url->urls[0])))
		return -ENOMEM;
	
	url->urls[url->count].uri = parser->content;
	DASH_TAG_ATTR_VALUE(attrs[0], ATTR_VALUE_TYPE_STRING, "serviceLocation", &url->urls[url->count].service_location);
	DASH_TAG_ATTR_VALUE(attrs[1], ATTR_VALUE_TYPE_STRING, "byteRange", &url->urls[url->count].byte_range);
	DASH_TAG_ATTR_VALUE(attrs[2], ATTR_VALUE_TYPE_FLOAT64, "availabilityTimeOffset", &url->urls[url->count].availability_time_offset);
	DASH_TAG_ATTR_VALUE(attrs[3], ATTR_VALUE_TYPE_STRING_BOOL, "availabilityTimeComplete", &url->urls[url->count].availability_time_complete);

	url->count++;
	return dash_parse_attrs(attr, len, attrs, sizeof(attrs) / sizeof(attrs[0]));
}

static int dash_tag_urltype(struct dash_parser_t* parser, void* ptr, const char* attr, size_t len)
{
	struct dash_tag_attr_t attrs[2];
	struct dash_urltype_t* url;
	url = (struct dash_urltype_t*)ptr;
	parser->tag = url; // save

	DASH_TAG_ATTR_VALUE(attrs[0], ATTR_VALUE_TYPE_STRING, "sourceURL", &url->source_url);
	DASH_TAG_ATTR_VALUE(attrs[1], ATTR_VALUE_TYPE_STRING, "range", &url->range);
	return dash_parse_attrs(attr, len, attrs, sizeof(attrs) / sizeof(attrs[0]));
}

static int dash_tag_event_stream_parse(struct dash_event_stream_t* stream, const char* attr, size_t len)
{
	struct dash_tag_attr_t attrs[5];
	stream->actuate = "onRequest"; // default
	DASH_TAG_ATTR_VALUE(attrs[0], ATTR_VALUE_TYPE_STRING, "xlink:href", &stream->href);
	DASH_TAG_ATTR_VALUE(attrs[1], ATTR_VALUE_TYPE_STRING, "xlink:actuate", &stream->actuate);
	DASH_TAG_ATTR_VALUE(attrs[2], ATTR_VALUE_TYPE_STRING, "schemeIdUri", &stream->scheme_id_uri);
	DASH_TAG_ATTR_VALUE(attrs[3], ATTR_VALUE_TYPE_STRING, "value", &stream->value);
	DASH_TAG_ATTR_VALUE(attrs[4], ATTR_VALUE_TYPE_UINT32, "timescale", &stream->timescale);
	return dash_parse_attrs(attr, len, attrs, sizeof(attrs) / sizeof(attrs[0]));
}

static int dash_tag_event_stream(struct dash_parser_t* parser, void* ptr, const char* attr, size_t len)
{
	int r;
	struct dash_event_stream_t* stream;
	struct dash_period_t* period;

	period = (struct dash_period_t*)ptr;
	if (0 != dash_parser_realloc((void**)&period->event_streams, &period->event_stream_capacity, period->event_stream_count, 2, sizeof(period->event_streams[0])))
		return -ENOMEM;

	stream = &period->event_streams[period->event_stream_count];
	r = dash_tag_event_stream_parse(stream, attr, len);
	if (0 != r)
		return r;

	parser->tag = stream; // save
	period->event_stream_count++;
	return 0;
}

static int dash_tag_event(struct dash_parser_t* parser, void* ptr, const char* attr, size_t len)
{
	int r;
	size_t capacity;
	struct dash_tag_attr_t attrs[4];
	struct dash_event_stream_t* stream;
	struct dash_event_t* event;

	stream = (struct dash_event_stream_t*)ptr;
	capacity = EVENT_CAPACITY(stream->event_count);
	if (0 != dash_parser_realloc((void**)&stream->events, &capacity, stream->event_count, EVENT_CAPACITY_INCR, sizeof(stream->events[0])))
		return -ENOMEM;

	event = &stream->events[stream->event_count];
	DASH_TAG_ATTR_VALUE(attrs[0], ATTR_VALUE_TYPE_UINT64, "presentationTime", &event->presentation_time);
	DASH_TAG_ATTR_VALUE(attrs[1], ATTR_VALUE_TYPE_UINT64, "duration", &event->duration);
	DASH_TAG_ATTR_VALUE(attrs[2], ATTR_VALUE_TYPE_UINT32, "id", &event->id);
	DASH_TAG_ATTR_VALUE(attrs[3], ATTR_VALUE_TYPE_STRING, "messageData", &event->message_data);
	
	r = dash_parse_attrs(attr, len, attrs, sizeof(attrs) / sizeof(attrs[0]));
	if (0 != r)
		return r;

	parser->tag = event; // save
	stream->event_count++;
	return 0;
}

static int dash_tag_content_component(struct dash_parser_t* parser, void* ptr, const char* attr, size_t len)
{
	int r;
	size_t capacity;
	struct dash_tag_attr_t attrs[5];
	struct dash_content_component_t* component;
	struct dash_adaptation_set_t* adaptation_set;

	adaptation_set = (struct dash_adaptation_set_t*)ptr;
	capacity = CONTENT_COMPONENT_CAPACITY(adaptation_set->content_component_count);
	if (0 != dash_parser_realloc((void**)&adaptation_set->content_components, &capacity, adaptation_set->content_component_count, CONTENT_COMPONENT_CAPACITY_INCR, sizeof(adaptation_set->content_components[0])))
		return -ENOMEM;

	component = &adaptation_set->content_components[adaptation_set->content_component_count];
	DASH_TAG_ATTR_VALUE(attrs[0], ATTR_VALUE_TYPE_UINT32, "id", &component->id);
	DASH_TAG_ATTR_VALUE(attrs[1], ATTR_VALUE_TYPE_STRING, "lang", &component->lang);
	DASH_TAG_ATTR_VALUE(attrs[2], ATTR_VALUE_TYPE_STRING, "contentType", &component->content_type);
	DASH_TAG_ATTR_VALUE(attrs[3], ATTR_VALUE_TYPE_STRING, "par", &component->par);
	DASH_TAG_ATTR_VALUE(attrs[4], ATTR_VALUE_TYPE_STRING, "tag", &component->tag);
	
	r = dash_parse_attrs(attr, len, attrs, sizeof(attrs) / sizeof(attrs[0]));
	if (0 != r)
		return r;

	parser->tag = component; // save
	adaptation_set->content_component_count++;
	return 0;
}

static int dash_tag_subset(struct dash_parser_t* parser, void* ptr, const char* attr, size_t len)
{
	int r;
	size_t capacity;
	struct dash_tag_attr_t attrs[2];
	struct dash_period_t* period;

	period = (struct dash_period_t*)ptr;
	capacity = SUBSET_CAPACITY(period->preselection_count);
	if (0 != dash_parser_realloc((void**)&period->subsets, &capacity, period->subset_count, SUBSET_INCR, sizeof(period->subsets[0])))
		return -ENOMEM;

	DASH_TAG_ATTR_VALUE(attrs[0], ATTR_VALUE_TYPE_STRING, "contains", &period->subsets[period->subset_count].contains);
	DASH_TAG_ATTR_VALUE(attrs[1], ATTR_VALUE_TYPE_STRING, "id", &period->subsets[period->subset_count].id);

	r = dash_parse_attrs(attr, len, attrs, sizeof(attrs) / sizeof(attrs[0]));
	if (0 != r)
		return r;

	parser->tag = &period->subsets[period->subset_count]; // save
	period->subset_count++;
	return 0;
}

static int dash_tag_switching(struct dash_parser_t* parser, void* ptr, const char* attr, size_t len)
{
	int r;
	size_t capacity;
	struct dash_tag_attr_t attrs[2];
	struct dash_representation_base_t* base;

	base = (struct dash_representation_base_t*)ptr;
	capacity = base->switching_count;
	if (0 != dash_parser_realloc((void**)&base->switchings, &capacity, base->switching_count, 1, sizeof(base->switchings[0])))
		return -ENOMEM;

	DASH_TAG_ATTR_VALUE(attrs[0], ATTR_VALUE_TYPE_UINT32, "interval", &base->switchings[base->switching_count].interval);
	DASH_TAG_ATTR_VALUE(attrs[1], ATTR_VALUE_TYPE_STRING, "type", &base->switchings[base->switching_count].type);

	r = dash_parse_attrs(attr, len, attrs, sizeof(attrs) / sizeof(attrs[0]));
	if (0 != r)
		return r;

	parser->tag = &base->switchings[base->switching_count]; // save
	base->switching_count++;
	return 0;
}

static int dash_tag_random_access(struct dash_parser_t* parser, void* ptr, const char* attr, size_t len)
{
	int r;
	size_t capacity;
	struct dash_tag_attr_t attrs[4];
	struct dash_representation_base_t* base;

	base = (struct dash_representation_base_t*)ptr;
	capacity = base->random_access_count;
	if (0 != dash_parser_realloc((void**)&base->random_accesses, &capacity, base->random_access_count, 1, sizeof(base->random_accesses[0])))
		return -ENOMEM;

	DASH_TAG_ATTR_VALUE(attrs[0], ATTR_VALUE_TYPE_UINT32, "interval", &base->random_accesses[base->random_access_count].interval);
	DASH_TAG_ATTR_VALUE(attrs[1], ATTR_VALUE_TYPE_STRING, "type", &base->random_accesses[base->random_access_count].type);
	DASH_TAG_ATTR_VALUE(attrs[0], ATTR_VALUE_TYPE_DURATION, "miniBufferTime", &base->random_accesses[base->random_access_count].min_buffer_time);
	DASH_TAG_ATTR_VALUE(attrs[0], ATTR_VALUE_TYPE_UINT32, "bandwidth", &base->random_accesses[base->random_access_count].bandwidth);

	r = dash_parse_attrs(attr, len, attrs, sizeof(attrs) / sizeof(attrs[0]));
	if (0 != r)
		return r;

	parser->tag = &base->random_accesses[base->random_access_count]; // save
	base->random_access_count++;
	return 0;
}

static int dash_tag_program_information(struct dash_parser_t* parser, void* ptr, const char* attr, size_t len)
{
	int r;
	size_t capacity;
	struct dash_tag_attr_t attrs[2];
	struct dash_mpd_t* mpd;
	struct dash_program_information_t* info;

	mpd = (struct dash_mpd_t*)ptr;
	capacity = mpd->info_count;
	if (0 != dash_parser_realloc((void**)&mpd->infos, &capacity, mpd->info_count, 1, sizeof(mpd->infos[0])))
		return -ENOMEM;

	info = &mpd->infos[mpd->info_count];
	DASH_TAG_ATTR_VALUE(attrs[0], ATTR_VALUE_TYPE_STRING, "lang", &info->lang);
	DASH_TAG_ATTR_VALUE(attrs[1], ATTR_VALUE_TYPE_STRING, "moreInformationURL", &info->more_information);

	r = dash_parse_attrs(attr, len, attrs, sizeof(attrs) / sizeof(attrs[0]));
	if (0 != r)
		return r;

	parser->tag = info; // save
	mpd->info_count++;
	return 0;
}

static int dash_tag_title(struct dash_parser_t* parser, void* ptr, const char* attr, size_t len)
{
	struct dash_program_information_t* info;
	info = (struct dash_program_information_t*)ptr;
	info->title = parser->content;
	(void)attr, (void)len;
	return 0;
}

static int dash_tag_source(struct dash_parser_t* parser, void* ptr, const char* attr, size_t len)
{
	struct dash_program_information_t* info;
	info = (struct dash_program_information_t*)ptr;
	info->source = parser->content;
	(void)attr, (void)len;
	return 0;
}

static int dash_tag_copyright(struct dash_parser_t* parser, void* ptr, const char* attr, size_t len)
{
	struct dash_program_information_t* info;
	info = (struct dash_program_information_t*)ptr;
	info->copyright = parser->content;
	(void)attr, (void)len;
	return 0;
}

static int dash_tag_location(struct dash_parser_t* parser, void* ptr, const char* attr, size_t len)
{
	size_t capacity;
	struct dash_mpd_t* mpd;
	mpd = (struct dash_mpd_t*)ptr;
	capacity = mpd->location_count;
	if (0 != dash_parser_realloc((void**)&mpd->locations, &capacity, mpd->location_count, 1, sizeof(mpd->locations[0])))
		return -ENOMEM;
	
	parser->tag = &mpd->locations[mpd->location_count]; // save
	mpd->locations[mpd->location_count++] = parser->content;
	(void)attr, (void)len;
	return 0;
}

static int dash_tag_descritor(struct dash_parser_t* parser, void* ptr, const char* attr, size_t len)
{
	size_t capacity;
	struct dash_tag_attr_t attrs[3];
	struct dash_descriptor_t* desc;

	desc = (struct dash_descriptor_t*)ptr;
	capacity = desc->count;
	if (0 != dash_parser_realloc((void**)&desc->descs, &capacity, desc->count, 1, sizeof(desc->descs[0])))
		return -ENOMEM;

	DASH_TAG_ATTR_VALUE(attrs[0], ATTR_VALUE_TYPE_STRING, "schemeIdUri", &desc->descs[desc->count].scheme_uri);
	DASH_TAG_ATTR_VALUE(attrs[1], ATTR_VALUE_TYPE_STRING, "value", &desc->descs[desc->count].value);
	DASH_TAG_ATTR_VALUE(attrs[2], ATTR_VALUE_TYPE_STRING, "id", &desc->descs[desc->count].id);
	
	parser->tag = &desc->descs[desc->count]; // save
	desc->count++;
	return dash_parse_attrs(attr, len, attrs, sizeof(attrs) / sizeof(attrs[0]));
}

static int dash_tag_metrics(struct dash_parser_t* parser, void* ptr, const char* attr, size_t len)
{
	int r;
	size_t capacity;
	struct dash_tag_attr_t attrs[2];
	struct dash_mpd_t* mpd;
	struct dash_metric_t* metric;

	mpd = (struct dash_mpd_t*)ptr;
	capacity = mpd->metric_count;
	if (0 != dash_parser_realloc((void**)&mpd->metrics, &capacity, mpd->metric_count, 1, sizeof(mpd->metrics[0])))
		return -ENOMEM;

	metric = &mpd->metrics[mpd->metric_count];
	DASH_TAG_ATTR_VALUE(attrs[0], ATTR_VALUE_TYPE_STRING, "metrics", &metric->metrics);
	
	r = dash_parse_attrs(attr, len, attrs, sizeof(attrs) / sizeof(attrs[0]));
	if (0 != r)
		return r;

	parser->tag = metric; // save
	mpd->metric_count++;
	return 0;
}

static int dash_tag_metric_range(struct dash_parser_t* parser, void* ptr, const char* attr, size_t len)
{
	int r;
	size_t capacity;
	struct dash_tag_attr_t attrs[2];
	struct dash_metric_t* metric;

	metric = (struct dash_metric_t*)ptr;
	capacity = metric->range_count;
	if (0 != dash_parser_realloc((void**)&metric->ranges, &capacity, metric->range_count, 1, sizeof(metric->ranges[0])))
		return -ENOMEM;

	DASH_TAG_ATTR_VALUE(attrs[0], ATTR_VALUE_TYPE_DURATION, "time", &metric->ranges[metric->range_count].time);
	DASH_TAG_ATTR_VALUE(attrs[0], ATTR_VALUE_TYPE_DURATION, "duration", &metric->ranges[metric->range_count].duration);

	r = dash_parse_attrs(attr, len, attrs, sizeof(attrs) / sizeof(attrs[0]));
	if (0 != r)
		return r;

	parser->tag = &metric->ranges[metric->range_count]; // save
	metric->range_count++;
	return 0;
}

static int dash_tag_label(struct dash_parser_t* parser, void* ptr, const char* attr, size_t len)
{
	size_t capacity;
	struct dash_tag_attr_t attrs[2];
	struct dash_label_t* label;

	label = (struct dash_label_t*)ptr;
	capacity = label->count;
	if (0 != dash_parser_realloc((void**)&label->labels, &capacity, label->count, 1, sizeof(label->labels[0])))
		return -ENOMEM;

	DASH_TAG_ATTR_VALUE(attrs[0], ATTR_VALUE_TYPE_UINT32, "id", &label->labels[label->count].id);
	DASH_TAG_ATTR_VALUE(attrs[1], ATTR_VALUE_TYPE_STRING, "lang", &label->labels[label->count].lang);

	parser->tag = &label->labels[label->count++]; // save
	return dash_parse_attrs(attr, len, attrs, sizeof(attrs) / sizeof(attrs[0]));
}

static int dash_tag_inband_event_stream(struct dash_parser_t* parser, void* ptr, const char* attr, size_t len)
{
	int r;
	size_t capacity;
	struct dash_event_stream_t* stream;
	struct dash_representation_base_t* base;

	base = (struct dash_representation_base_t*)ptr;
	capacity = base->inband_event_stream_count;
	if (0 != dash_parser_realloc((void**)&base->inband_event_streams, &capacity, base->inband_event_stream_count, 1, sizeof(base->inband_event_streams[0])))
		return -ENOMEM;

	stream = &base->inband_event_streams[base->inband_event_stream_count];
	r = dash_tag_event_stream_parse(stream, attr, len);
	if (0 != r)
		return r;

	parser->tag = stream; // save
	base->inband_event_stream_count++;
	return 0;
}

static const struct
{
	const char* name;
	const char* parent;
	int (*parser)(struct dash_parser_t* parser, void* ptr, const char* attr, size_t len);
	size_t offset;
} s_tags[] = {
	// MPD
	{ "MPD",						"",		dash_tag_mpd, 0 },
	{ "ProgramInformation",			"MPD",	dash_tag_program_information, 0 },
	{ "BaseURL",					"MPD",	dash_tag_baseurl, offsetof(struct dash_mpd_t, urls) },
	{ "Location",					"MPD",	dash_tag_location, 0 },
	{ "Period",						"MPD",	dash_tag_period, 0 },
	{ "Metrics",					"MPD",	dash_tag_metrics, 0 },
	{ "EssentialProperty",			"MPD",	dash_tag_descritor, offsetof(struct dash_mpd_t, essentials) },
	{ "SupplementalProperty",		"MPD",	dash_tag_descritor, offsetof(struct dash_mpd_t, supplementals) },
	{ "UTCTiming",					"MPD",	dash_tag_descritor, offsetof(struct dash_mpd_t, timings) },

	// Period
	{ "BaseURL",					"Period",	dash_tag_baseurl, offsetof(struct dash_period_t, base_urls) },
	{ "SegmentBase",				"Period",	dash_tag_segment_base, offsetof(struct dash_period_t, segment) },
	{ "SegmentList",				"Period",	dash_tag_segment_list, offsetof(struct dash_period_t, segment) },
	{ "SegmentTemplate",			"Period",	dash_tag_segment_template, offsetof(struct dash_period_t, segment) },
	{ "AssetIdentifier",			"Period",	dash_tag_descritor, offsetof(struct dash_period_t, asset_identifier) },
	{ "EventStream",				"Period",	dash_tag_event_stream, 0 },
	{ "AdaptationSet",				"Period",	dash_tag_adaptation_set, 0 },
	{ "Subset",						"Period",	dash_tag_subset, 0 },
	{ "SupplementalProperty",		"Period",	dash_tag_descritor, offsetof(struct dash_period_t, supplementals) },
	{ "EmptyAdaptationSet",			"Period",	dash_tag_empty_adaptation_set, 0 },
	{ "GroupLabel",					"Period",	dash_tag_label, offsetof(struct dash_period_t, group_labels) },
	{ "Preselection",				"Period",	dash_tag_preselection, 0 },

	// EventStream
	{ "Event",						"EventStream",		dash_tag_event, 0 },

	// Adaptation Set
	{ "FramePacking",				"AdaptationSet",	dash_tag_descritor, offsetof(struct dash_adaptation_set_t, base) + offsetof(struct dash_representation_base_t, frame_packings) },
	{ "AudioChannelConfiguration",	"AdaptationSet",	dash_tag_descritor, offsetof(struct dash_adaptation_set_t, base) + offsetof(struct dash_representation_base_t, audio_channel_configurations) },
	{ "ContentProtection",			"AdaptationSet",	dash_tag_descritor, offsetof(struct dash_adaptation_set_t, base) + offsetof(struct dash_representation_base_t, content_protections) },
	{ "EssentialProperty",			"AdaptationSet",	dash_tag_descritor, offsetof(struct dash_adaptation_set_t, base) + offsetof(struct dash_representation_base_t, essentials) },
	{ "SupplementalProperty",		"AdaptationSet",	dash_tag_descritor, offsetof(struct dash_adaptation_set_t, base) + offsetof(struct dash_representation_base_t, supplementals) },
	{ "InbandEventSteram",			"AdaptationSet",	dash_tag_inband_event_stream, offsetof(struct dash_adaptation_set_t, base) },
	{ "Switching",					"AdaptationSet",	dash_tag_switching, offsetof(struct dash_adaptation_set_t, base) },
	{ "RandomAccess",				"AdaptationSet",	dash_tag_random_access, offsetof(struct dash_adaptation_set_t, base) },
	{ "GroupLabel",					"AdaptationSet",	dash_tag_label, offsetof(struct dash_adaptation_set_t, base) + offsetof(struct dash_representation_base_t, group_labels) },
	{ "Label",						"AdaptationSet",	dash_tag_label, offsetof(struct dash_adaptation_set_t, base) + offsetof(struct dash_representation_base_t, labels) },
	{ "Accessibility",				"AdaptationSet",	dash_tag_descritor, offsetof(struct dash_adaptation_set_t, accessibilities) },
	{ "Role",						"AdaptationSet",	dash_tag_descritor, offsetof(struct dash_adaptation_set_t, roles) },
	{ "Rating",						"AdaptationSet",	dash_tag_descritor, offsetof(struct dash_adaptation_set_t, ratings) },
	{ "Viewpoint",					"AdaptationSet",	dash_tag_descritor, offsetof(struct dash_adaptation_set_t, viewpoints) },
	{ "ContentComponent",			"AdaptationSet",	dash_tag_content_component, 0 },
	{ "BaseURL",					"AdaptationSet",	dash_tag_baseurl, offsetof(struct dash_adaptation_set_t, base_urls) },
	{ "SegmentBase",				"AdaptationSet",	dash_tag_segment_base, offsetof(struct dash_adaptation_set_t, segment) },
	{ "SegmentList",				"AdaptationSet",	dash_tag_segment_list, offsetof(struct dash_adaptation_set_t, segment) },
	{ "SegmentTemplate",			"AdaptationSet",	dash_tag_segment_template, offsetof(struct dash_adaptation_set_t, segment) },
	{ "Representation",				"AdaptationSet",	dash_tag_representation, 0 },

	// Content Component
	{ "Accessibility",				"ContentComponent",	dash_tag_descritor, offsetof(struct dash_content_component_t, accessibilities) },
	{ "Role",						"ContentComponent",	dash_tag_descritor, offsetof(struct dash_content_component_t, roles) },
	{ "Rating",						"ContentComponent",	dash_tag_descritor, offsetof(struct dash_content_component_t, ratings) },
	{ "Viewpoint",					"ContentComponent",	dash_tag_descritor, offsetof(struct dash_content_component_t, viewpoints) },

	// Representation
	{ "FramePacking",				"Representation",	dash_tag_descritor, offsetof(struct dash_representation_t, base) + offsetof(struct dash_representation_base_t, frame_packings) },
	{ "AudioChannelConfiguration",	"Representation",	dash_tag_descritor, offsetof(struct dash_representation_t, base) + offsetof(struct dash_representation_base_t, audio_channel_configurations) },
	{ "ContentProtection",			"Representation",	dash_tag_descritor, offsetof(struct dash_representation_t, base) + offsetof(struct dash_representation_base_t, content_protections) },
	{ "EssentialProperty",			"Representation",	dash_tag_descritor, offsetof(struct dash_representation_t, base) + offsetof(struct dash_representation_base_t, essentials) },
	{ "SupplementalProperty",		"Representation",	dash_tag_descritor, offsetof(struct dash_representation_t, base) + offsetof(struct dash_representation_base_t, supplementals) },
	{ "InbandEventSteram",			"Representation",	dash_tag_inband_event_stream, offsetof(struct dash_representation_t, base) },
	{ "Switching",					"Representation",	dash_tag_switching, offsetof(struct dash_representation_t, base) },
	{ "RandomAccess",				"Representation",	dash_tag_random_access, offsetof(struct dash_representation_t, base) },
	{ "GroupLabel",					"Representation",	dash_tag_label, offsetof(struct dash_representation_t, base) + offsetof(struct dash_representation_base_t, group_labels) },
	{ "Label",						"Representation",	dash_tag_label, offsetof(struct dash_representation_t, base) + offsetof(struct dash_representation_base_t, labels) },
	{ "BaseURL",					"Representation",	dash_tag_baseurl, offsetof(struct dash_adaptation_set_t, base_urls) },
	{ "SubRepresentation",			"Representation",	dash_tag_subrepresentation, 0 },
	{ "SegmentBase",				"Representation",	dash_tag_segment_base, offsetof(struct dash_representation_t, segment) },
	{ "SegmentList",				"Representation",	dash_tag_segment_list, offsetof(struct dash_representation_t, segment) },
	{ "SegmentTemplate",			"Representation",	dash_tag_segment_template, offsetof(struct dash_representation_t, segment) },

	// Subresentation
	{ "FramePacking",				"SubRepresentation", dash_tag_descritor, offsetof(struct dash_subrepresentation_t, base) + offsetof(struct dash_representation_base_t, frame_packings) },
	{ "AudioChannelConfiguration",	"SubRepresentation", dash_tag_descritor, offsetof(struct dash_subrepresentation_t, base) + offsetof(struct dash_representation_base_t, audio_channel_configurations) },
	{ "ContentProtection",			"SubRepresentation", dash_tag_descritor, offsetof(struct dash_subrepresentation_t, base) + offsetof(struct dash_representation_base_t, content_protections) },
	{ "EssentialProperty",			"SubRepresentation", dash_tag_descritor, offsetof(struct dash_subrepresentation_t, base) + offsetof(struct dash_representation_base_t, essentials) },
	{ "SupplementalProperty",		"SubRepresentation", dash_tag_descritor, offsetof(struct dash_subrepresentation_t, base) + offsetof(struct dash_representation_base_t, supplementals) },
	{ "InbandEventSteram",			"SubRepresentation", dash_tag_inband_event_stream, offsetof(struct dash_subrepresentation_t, base) },
	{ "Switching",					"SubRepresentation", dash_tag_switching, offsetof(struct dash_subrepresentation_t, base) },
	{ "RandomAccess",				"SubRepresentation", dash_tag_random_access, offsetof(struct dash_subrepresentation_t, base) },
	{ "GroupLabel",					"SubRepresentation", dash_tag_label, offsetof(struct dash_subrepresentation_t, base) + offsetof(struct dash_representation_base_t, group_labels) },
	{ "Label",						"SubRepresentation", dash_tag_label, offsetof(struct dash_subrepresentation_t, base) + offsetof(struct dash_representation_base_t, labels) },

	// Preselection
	{ "FramePacking",				"Preselection",		dash_tag_descritor, offsetof(struct dash_preselection_t, base) + offsetof(struct dash_representation_base_t, frame_packings) },
	{ "AudioChannelConfiguration",	"Preselection",		dash_tag_descritor, offsetof(struct dash_preselection_t, base) + offsetof(struct dash_representation_base_t, audio_channel_configurations) },
	{ "ContentProtection",			"Preselection",		dash_tag_descritor, offsetof(struct dash_preselection_t, base) + offsetof(struct dash_representation_base_t, content_protections) },
	{ "EssentialProperty",			"Preselection",		dash_tag_descritor, offsetof(struct dash_preselection_t, base) + offsetof(struct dash_representation_base_t, essentials) },
	{ "SupplementalProperty",		"Preselection",		dash_tag_descritor, offsetof(struct dash_preselection_t, base) + offsetof(struct dash_representation_base_t, supplementals) },
	{ "InbandEventSteram",			"Preselection",		dash_tag_inband_event_stream, offsetof(struct dash_preselection_t, base) },
	{ "Switching",					"Preselection",		dash_tag_switching, offsetof(struct dash_preselection_t, base) },
	{ "RandomAccess",				"Preselection",		dash_tag_random_access, offsetof(struct dash_preselection_t, base) },
	{ "GroupLabel",					"Preselection",		dash_tag_label, offsetof(struct dash_preselection_t, base) + offsetof(struct dash_representation_base_t, group_labels) },
	{ "Label",						"Preselection",		dash_tag_label, offsetof(struct dash_preselection_t, base) + offsetof(struct dash_representation_base_t, labels) },
	{ "Accessibility",				"Preselection",		dash_tag_descritor, offsetof(struct dash_preselection_t, accessibilities) },
	{ "Role",						"Preselection",		dash_tag_descritor, offsetof(struct dash_preselection_t, roles) },
	{ "Rating",						"Preselection",		dash_tag_descritor, offsetof(struct dash_preselection_t, ratings) },
	{ "Viewpoint",					"Preselection",		dash_tag_descritor, offsetof(struct dash_preselection_t, viewpoints) },

	// SegmentBase
	{ "Initialization",				"SegmentBase",		dash_tag_urltype, offsetof(struct dash_segment_t, initialization) },
	{ "RepresentationIndex",		"SegmentBase",		dash_tag_urltype, offsetof(struct dash_segment_t, representation_index) },

	// SegmentList
	{ "Initialization",				"SegmentList",		dash_tag_urltype, offsetof(struct dash_segment_t, initialization) },
	{ "RepresentationIndex",		"SegmentList",		dash_tag_urltype, offsetof(struct dash_segment_t, representation_index) },
	{ "SegmentTimeline",			"SegmentList",		dash_tag_segment_timeline, 0 },
	{ "BitstreamSwitching",			"SegmentList",		dash_tag_urltype, offsetof(struct dash_segment_t, bitstream_switching) },
	{ "SegmentUrl",					"SegmentList",		dash_tag_segment_url, 0 },
	
	// SegmentTemplate
	{ "Initialization",				"SegmentTemplate",	dash_tag_urltype, offsetof(struct dash_segment_t, initialization) },
	{ "RepresentationIndex",		"SegmentTemplate",	dash_tag_urltype, offsetof(struct dash_segment_t, representation_index) },
	{ "SegmentTimeline",			"SegmentTemplate",	dash_tag_segment_timeline, 0 },
	{ "BitstreamSwitching",			"SegmentTemplate",	dash_tag_urltype, offsetof(struct dash_segment_t, bitstream_switching) },
	
	// SegmentTimeline
	{ "S",							"SegmentTimeline",	dash_tag_s, 0 },

	// Program Information
	{ "Title",						"ProgramInformation", dash_tag_title, 0 },
	{ "Source",						"ProgramInformation", dash_tag_source, 0 },
	{ "CopyRight",					"ProgramInformation", dash_tag_copyright, 0 },

	// Metrics
	{ "Range",						"Metrics",			dash_tag_metric_range, 0 },
	{ "Reporting",					"Metrics",			dash_tag_descritor, offsetof(struct dash_metric_t, reportings) },
};

static int dash_parser_ontag(struct dash_parser_t* parser, const char* tag, const char* attrs, size_t bytes)
{
	int r;
	size_t i;
	void* ptr;
	const char* parent;

	r = 0;
	parent = parser->level > 0 ? parser->stack[parser->level - 1].tag : "";
	//printf("%.*s: %.*s %.*s\n", (int)len, tag, (int)bytes, attrs, (int)ncontent, content);
	for (i = 0; i < sizeof(s_tags) / sizeof(s_tags[0]) && 0 == r; i++)
	{
		if (0 == strcasecmp(s_tags[i].name, tag) && 0 == strcasecmp(s_tags[i].parent, parent))
		{
			ptr = parser->level > 0 ? parser->stack[parser->level - 1].ptr : NULL;
			ptr = (uint8_t*)ptr + s_tags[i].offset;
			r = s_tags[i].parser(parser, ptr, attrs, bytes);
			break;
		}
	}

	return r;
}

static const char* dash_parser_tag(struct dash_tag_t* tag, const char* data, const char* end)
{
	const char* next;
	
	memset(tag, 0, sizeof(*tag));
	tag->ptr = strchr(data, '<');
	if (!tag->ptr || tag->ptr >= end)
		return end; // all done

	tag->nlen = end - tag->ptr - 1;
	tag->name = hls_strtrim(tag->ptr + 1 /* skip '<' */, &tag->nlen, " \t\r\n", NULL); // trim SP/HTAB
	switch (*tag->name)
	{
	case '?':
		// XML declaration
		// <?xml version="1.0" encoding="utf-8"?>
		tag->type = DASH_TAG_DECLARATION;
		tag->name += 1; // skip '?'
		tag->nlen -= 1;
		break;

	case '/':
		tag->flags |= DASH_TAG_FLAG_END;
		tag->type = DASH_TAG_ELEMENT;
		tag->name += 1; // skip '/'
		tag->nlen -= 1;
		break;

	case '!':
		if (tag->nlen >= 3 && 0 == strncmp("!--", tag->name, 3))
		{
			tag->type = DASH_TAG_COMMENT;
			next = strstr(tag->name + 3, "-->");
			tag->end = next ? next + 2 : end;
			return next ? next + 3 : end;
		}
		else if (tag->nlen >= 8 && 0 == strncmp("![CDATA[", tag->name, 8))
		{
			tag->type = DASH_TAG_CDATA;
			next = strstr(tag->name + 8, "]]>");
			tag->end = next ? next + 2 : end;
			return next ? next + 3 : end;
		}
		else
		{
			// unknown
			assert(0);
			return NULL;
		}
		break;

	default:
		tag->flags |= DASH_TAG_FLAG_START;
		tag->type = DASH_TAG_ELEMENT;
		break;
	}

	tag->nlen = hls_strsplit(tag->name, end, ">", "\'\"", &next);
	tag->end = tag->name + tag->nlen;
	tag->name = hls_strtrim(tag->name, &tag->nlen, " \t\r\n", " \t\r\n"); // trim SP/HTAB
	if (tag->nlen > 0 && '/' == tag->name[tag->nlen - 1])
	{
		// line-break
		assert(0 == (tag->flags & DASH_TAG_FLAG_END));
		tag->flags |= DASH_TAG_FLAG_LINBREAK;
		tag->nlen -= 1;
	}

	// attributes
	tag->attr = strpbrk(tag->name, "/> \t\r\n");
	assert(tag->attr <= tag->name + tag->nlen);
	tag->nattr = tag->nlen - (tag->attr - tag->name);
	tag->nlen = tag->attr ? tag->attr - tag->name : tag->nlen;
	tag->attr = hls_strtrim(tag->attr, &tag->nattr, " \t\r\n", " \t\r\n"); // trim SP/HTAB

	if (tag->nlen < 1)
	{
		assert(0);
		return NULL;
	}
	return next;
}

static int dash_parser_input(struct dash_parser_t* parser, const char* data, size_t bytes)
{
	int r;
	size_t ncontent;
	const char* ptr, *next;
	struct dash_tag_t tag, tag2;

	r = -1;
	assert(data && bytes > 0);
	next = dash_parser_tag(&tag, data, data + bytes);
	for (ptr = next; ptr && ptr < data + bytes; ptr = next)
	{
		next = dash_parser_tag(&tag2, ptr, data + bytes);

		if (DASH_TAG_ELEMENT != tag.type)
		{
			memcpy(&tag, &tag2, sizeof(tag));
			continue; // ignore declaration/comments/cdata
		}

		assert( /*'<' == *tag.ptr &&*/ '>' == *tag.end);
		assert(tag.name && tag.nlen > 0);
		((char*)tag.name)[tag.nlen] = '\0';

		if (DASH_TAG_FLAG_END & tag.flags)
		{
			assert(0 == ((DASH_TAG_FLAG_LINBREAK | DASH_TAG_FLAG_START) & tag.flags));
			if (parser->level < 1 || 0 != strcasecmp(parser->stack[parser->level - 1].tag, tag.name))
			{
				assert(0);
				return -1; // tag don't match
			}			
			parser->level -= 1;
		}
		else
		{
			assert(DASH_TAG_FLAG_START & tag.flags);
			if (0 == (DASH_TAG_FLAG_LINBREAK & tag.flags))
			{
				ncontent = tag2.ptr - ptr;
				parser->content = (char*)hls_strtrim(ptr, &ncontent, " \t\r\n", " \t\r\n"); // trim SP/HTAB
				parser->content[ncontent] = '\0';
			}
			else
			{
				parser->content = "";
			}

			r = dash_parser_ontag(parser, tag.name, tag.attr, tag.nattr);

			assert(parser->level < sizeof(parser->stack) / sizeof(parser->stack[0]));
			if (0 == (DASH_TAG_FLAG_LINBREAK & tag.flags) && parser->level < sizeof(parser->stack) / sizeof(parser->stack[0]))
			{
				parser->stack[parser->level].tag = tag.name;
				parser->stack[parser->level].ptr = parser->tag;
				parser->level += 1;
			}
		}

		memcpy(&tag, &tag2, sizeof(tag));
	}

	if (!ptr)
		return -1;
	return r;
}

int dash_mpd_parse(struct dash_mpd_t** mpd, const char* data, size_t bytes)
{
	int r;
	char* ptr;
	struct dash_parser_t parser;

	if (!data || bytes < 6 /* <MPD/> */)
		return -1;

	memset(&parser, 0, sizeof(parser));
	parser.mpd = (struct dash_mpd_t*)calloc(1, sizeof(struct dash_mpd_t) + bytes + 1);
	if (!parser.mpd)
		return -ENOMEM;

	ptr = (char*)(parser.mpd + 1);
	memcpy(ptr, data, bytes);

	r = dash_parser_input(&parser, ptr, bytes);
	if (0 != r)
	{
		dash_mpd_free(&parser.mpd);
		return r;
	}

	*mpd = parser.mpd;
	return 0;
}

static void dash_segment_free(struct dash_segment_t* segment)
{
	if (segment->segment_timeline.S)
		free(segment->segment_timeline.S);
	if (segment->segment_urls)
		free(segment->segment_urls);
}

static void dash_representation_base_free(struct dash_representation_base_t* base)
{
	size_t i;

	if (base->frame_packings.descs)
		free(base->frame_packings.descs);
	if (base->audio_channel_configurations.descs)
		free(base->audio_channel_configurations.descs);
	if (base->content_protections.descs)
		free(base->content_protections.descs);
	if (base->essentials.descs)
		free(base->essentials.descs);
	if (base->supplementals.descs)
		free(base->supplementals.descs);

	for (i = 0; i < base->inband_event_stream_count; i++)
	{
		if (base->inband_event_streams[i].events)
			free(base->inband_event_streams[i].events);
	}
	if (base->inband_event_streams)
		free(base->inband_event_streams);

	if (base->switchings)
		free(base->switchings);
	if (base->random_accesses)
		free(base->random_accesses);
	if (base->group_labels.labels)
		free(base->group_labels.labels);
	if (base->labels.labels)
		free(base->labels.labels);
}

static void dash_representation_free(struct dash_representation_t* representation)
{
	size_t i;

	dash_representation_base_free(&representation->base);

	if (representation->base_urls.urls)
		free(representation->base_urls.urls);

	for (i = 0; i < representation->subrepresentation_count; i++)
	{
		dash_representation_base_free(&representation->subrepresentations[i].base);
	}
	if (representation->subrepresentations)
		free(representation->subrepresentations);

	dash_segment_free(&representation->segment);
}

static void dash_adaptation_set_free(struct dash_adaptation_set_t* set)
{
	size_t k;

	dash_representation_base_free(&set->base);
	if (set->accessibilities.descs)
		free(set->accessibilities.descs);
	if (set->roles.descs)
		free(set->roles.descs);
	if (set->ratings.descs)
		free(set->ratings.descs);
	if (set->viewpoints.descs)
		free(set->viewpoints.descs);

	for (k = 0; k < set->content_component_count; k++)
	{
		if (set->content_components[k].accessibilities.descs)
			free(set->content_components[k].accessibilities.descs);
		if (set->content_components[k].roles.descs)
			free(set->content_components[k].roles.descs);
		if (set->content_components[k].ratings.descs)
			free(set->content_components[k].ratings.descs);
		if (set->content_components[k].viewpoints.descs)
			free(set->content_components[k].viewpoints.descs);
	}
	if (set->content_components)
		free(set->content_components);

	if (set->base_urls.urls)
		free(set->base_urls.urls);

	dash_segment_free(&set->segment);

	for (k = 0; k < set->content_component_count; k++)
	{
		dash_representation_free(&set->representations[k]);
	}
	if (set->representations)
		free(set->representations);
}

static void dash_period_free(struct dash_period_t* period)
{
	size_t j;

	dash_segment_free(&period->segment);
	if (period->base_urls.urls)
		free(period->base_urls.urls);
	if (period->asset_identifier.descs)
		free(period->asset_identifier.descs);

	for (j = 0; j < period->event_stream_count; j++)
	{
		if (period->event_streams[j].events)
			free(period->event_streams[j].events);
	}
	if (period->event_streams)
		free(period->event_streams);

	for (j = 0; j < period->adaptation_set_count; j++)
	{
		dash_adaptation_set_free(&period->adaptation_sets[j]);
	}
	if (period->adaptation_sets)
		free(period->adaptation_sets);

	for (j = 0; j < period->empty_adaptation_set_count; j++)
	{
		dash_adaptation_set_free(&period->empty_adaptation_sets[j]);
	}
	if (period->empty_adaptation_sets)
		free(period->empty_adaptation_sets);

	if (period->subsets)
		free(period->subsets);
	if (period->supplementals.descs)
		free(period->supplementals.descs);
	if (period->group_labels.labels)
		free(period->group_labels.labels);

	for (j = 0; j < period->preselection_count; j++)
	{
		dash_representation_base_free(&period->preselections[j].base);
		if (period->preselections[j].accessibilities.descs)
			free(period->preselections[j].accessibilities.descs);
		if (period->preselections[j].roles.descs)
			free(period->preselections[j].roles.descs);
		if (period->preselections[j].ratings.descs)
			free(period->preselections[j].ratings.descs);
		if (period->preselections[j].viewpoints.descs)
			free(period->preselections[j].viewpoints.descs);
	}
	if (period->preselections)
		free(period->preselections);
}

int dash_mpd_free(struct dash_mpd_t** mpd)
{
	size_t i;
	struct dash_mpd_t* p;

	if (mpd && *mpd)
	{
		p = *mpd;
		if (p->infos)
		{
			assert(p->info_count > 0);
			free(p->infos);
		}
		if (p->urls.urls)
		{
			assert(p->urls.count > 0);
			free(p->urls.urls);
		}
		if (p->locations)
		{
			assert(p->location_count > 0);
			free(p->locations);
		}

		for(i = 0; i < p->period_count; i++)
			dash_period_free(&p->periods[i]);
		if(p->periods)
			free(p->periods);

		for (i = 0; i < p->metric_count; i++)
		{
			if (p->metrics[i].ranges)
				free(p->metrics[i].ranges);
		}
		if(p->metrics)
			free(p->metrics);

		if (p->essentials.descs)
			free(p->essentials.descs);
		if (p->supplementals.descs)
			free(p->supplementals.descs);
		if (p->timings.descs)
			free(p->timings.descs);

		free(p);
		*mpd = NULL;
		return 0;
	}
	return -1;
}

#if defined(_DEBUG) || defined(DEBUG)
void dash_parser_test(const char* xml)
{
	static char data[2 * 1024 * 1024];
	FILE* fp = fopen(xml, "rb");
	int n = (int)fread(data, 1, sizeof(data), fp);
	fclose(fp);

	struct dash_mpd_t* mpd;
	assert(0 == dash_mpd_parse(&mpd, data, n));
	dash_mpd_free(&mpd);
}
#endif
