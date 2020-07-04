#include "dash-parser.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

int dash_segment_count(const struct dash_segment_t* segment)
{
	int n;
	size_t i;

	switch (segment->type)
	{
	case DASH_SEGMENT_BASE:
		return 1;

	case DASH_SEGMENT_LIST:
		return (int)segment->segment_url_count;

	case DASH_SEGMENT_TEMPLATE:
		if (segment->segment_timeline.count < 1)
			return INT_MAX; // dynamic + infinite

		// static + timeline
		for (i = n = 0; i < segment->segment_timeline.count; i++)
			n += 1 + segment->segment_timeline.S[i].r;
		return n;

	default:
		return 0; // none
	}
}

const char* dash_segment_initialization(const struct dash_segment_t* segment)
{
	if (segment->initialization.source_url && *segment->initialization.source_url)
		return segment->initialization.source_url;

	if (DASH_SEGMENT_TEMPLATE == segment->type && segment->initialization_url && *segment->initialization_url)
		return segment->initialization_url;

	return NULL;
}

/// @return 0-ok, <0-error
static int dash_segment_timeline(const struct dash_segment_timeline_t* timeline, size_t index, int64_t* number, int64_t* start, int64_t* duration)
{
	int64_t t;
	size_t i, j, step;

	t = 0;
	for (j = i = 0; i < timeline->count; i++)
	{
		// 5.3.9.6 Segment timeline
		// The @r attribute has a default value of zero when not present. 
		// The value of the @r attribute of the S element may be set to a 
		// negative value indicating that the duration indicated in @d is 
		// promised to repeat until the S@t of the next S element or if it 
		// is the last S element in the SegmentTimeline element until the 
		// end of the Period or the next update of the MPD

		assert(timeline->S[i].d > 0);
		if (timeline->S[i].r >= 0)
		{
			step = timeline->S[i].r + 1;
		}
		else if (i + 1 == timeline->count)
		{
			// last
			step = 0;
		}
		else
		{
			assert(timeline->S[i].t > 0);
			step = (size_t)((timeline->S[i + 1].t - t) / (timeline->S[i].d > 0 ? timeline->S[i].d : 1));
		}

		if (0 == step || index < j + step)
		{
			*number = timeline->S[i].n + (index - j);
			*start = (timeline->S[i].t ? timeline->S[i].t : t) + (index - j) * timeline->S[i].d;
			*duration = timeline->S[i].d;
			return 0;
		}

		j += step;
		t = (timeline->S[i].t ? timeline->S[i].t : t) + step * timeline->S[i].d;
	}

	return -1;
}

/// @param[out] number start number of representation
/// @param[out] start start time of representation(MUST add period.start)
/// @param[out] duration segment duration, 0 if unknown
/// @param[out] url segment url(MUST resolve with representation base url)
/// @param[out] range url range, NULL if don't have range
/// @return 0-ok, <0-error, >0-undefined
int dash_segment_information(const struct dash_segment_t* segment, int index, int64_t* number, int64_t* start, int64_t* duration, const char** url, const char** range)
{
	int r;
	int64_t timescale;
	static const char* sc_empty = "";

	// 5.3.9.2 Segment base information
	// 1. If the Representation contains more than one Media Segment, then either 
	//    the attribute @duration or the element SegmentTimeline shall be present. 
	// 2. The attribute @duration and the element SegmentTimeline shall not be present at the same time.

	// 5.3.9.5.3 Media Segment information
	// 1. a valid Media Segment URL and possibly a byte range,
	// 2. the number and position of the Media Segment in the Representation,
	// 3. the MPD start time of the Media Segment in the Representation providing an approximate presentation start time of the Segment in the Period,
	// 4. the MPD duration of the Media Segment providing an approximate presentation duration of the Segment.
	//
	// SegmentTemplate
	// 1. If the Representation contains or inherits a SegmentTemplate element with $Number$ 
	//    then the URL of the Media Segment at position k in the Representation is determined 
	//    by replacing the $Number$ identifier by (k-1) + (k.start-1) with kstart the value of 
	//    the @startNumber attribute, if present, or 1 otherwise.
	// 2. If the Representation contains or inherits a SegmentTemplate element with $Time$ then 
	//    the URL of the media segment at position k is determined by replacing the $Time$ 
	//    identifier by MPD start time of this segment, as described below.
	//
	// SegmentList
	// The number of the first Segment in the list within this Period is determined by the value 
	// of the SegmentList@startNumber attribute, if present, or it is 1 in case this attribute 
	// is not present.The sequence of multiple SegmentList elements within a Representation shall 
	// result in Media Segment List with consecutive numbers.

	r = 0;
	
	if (DASH_SEGMENT_BASE == segment->type)
	{
		if (0 != index)
			return -1; // segment base only have one segment

		*number = 0;
		*start = 0 - segment->presentation_time_offset;
		*duration = segment->presentation_duration;
		*url = sc_empty;
	}
	else
	{
		if (index < 0 || (DASH_SEGMENT_LIST == segment->type && index >= (int)segment->segment_url_count))
			return -1;

		if (segment->segment_timeline.count > 0)
		{
			r = dash_segment_timeline(&segment->segment_timeline, index, number, start, duration);
			if (0 != r)
				return -1;
		}
		else
		{
			*number = (segment->start_number > 0 ? (segment->start_number - 1) : 0) + index;
			*start = (int64_t)index * segment->duration - segment->presentation_time_offset;
			*duration = segment->duration;
		}

		if (DASH_SEGMENT_LIST == segment->type)
		{
			*url = segment->segment_urls[index].media;
			*range = segment->segment_urls[index].media_range;
		}
		else if (DASH_SEGMENT_TEMPLATE == segment->type)
		{
			*url = segment->media;
			*range = NULL;
		}
		else
		{
			assert(0);
			return -1;
		}
	}

	timescale = segment->timescale > 0 ? segment->timescale : 1;
	*start = *start / timescale;
	*duration = *duration / timescale;
	return r;
}

int dash_segment_find(const struct dash_segment_t* segment, int64_t time)
{
	int r, n, i, mid;
	int64_t number, t, d;
	const char* url, * range;

	n = dash_segment_count(segment);
	i = 0;
	mid = -1;

	while (i < n)
	{
		mid = (i + n) / 2;
		r = dash_segment_information(segment, mid, &number, &t, &d, &url, &range);
		if (0 != r)
			return r;

		if (time < t)
			n = mid;
		else if (time > t + d)
			i = mid + 1;
		else
			break;
	}

	return mid;
}
