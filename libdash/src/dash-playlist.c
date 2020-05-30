#include "dash-parser.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int dash_mime_type_to_media_type(const char* mime)
{
	if (0 == strncmp(mime, "audio/", 6))
		return DASH_MEDIA_AUDIO;
	else if (0 == strncmp(mime, "video/", 6))
		return DASH_MEDIA_VIDEO;
	else
		return DASH_MEDIA_UNKNOWN;
}

int dash_adaptation_set_get_media_type(const struct dash_adaptation_set_t* set)
{
	const struct dash_representation_base_t* base;
	
	base = &set->base;
	if (base->mime_type && *base->mime_type)
		return dash_mime_type_to_media_type(base->mime_type);

	if (set->representation_count > 0)
	{
		base = &set->representations[0].base;
		if (base->mime_type && *base->mime_type)
			return dash_mime_type_to_media_type(base->mime_type);

		if (set->representations[0].subrepresentation_count > 0)
		{
			base = &set->representations[0].subrepresentations[0].base;
			return dash_mime_type_to_media_type(base->mime_type);
		}
	}

	return DASH_MEDIA_UNKNOWN;
}

const struct dash_url_t* dash_representation_get_base_url(const struct dash_representation_t* representation)
{
	const struct dash_mpd_t* mpd;
	const struct dash_period_t* period;
	const struct dash_adaptation_set_t* set;

	if (representation->base_urls.count > 0)
		return &representation->base_urls;

	set = representation->parent;
	if (set->base_urls.count > 0)
		return &set->base_urls;

	period = set->parent;
	if (period->base_urls.count > 0)
		return &period->base_urls;

	mpd = period->parent;
	return &mpd->urls;
}

const struct dash_segment_t* dash_representation_get_segment(const struct dash_representation_t* representation)
{
	const struct dash_period_t* period;
	const struct dash_adaptation_set_t* set;

	if (DASH_SEGMENT_NONE != representation->segment.type)
		return &representation->segment;

	set = representation->parent;
	if (DASH_SEGMENT_NONE != set->segment.type)
		return &set->segment;

	period = set->parent;
	if (DASH_SEGMENT_NONE != period->segment.type)
		return &period->segment;

	return &representation->segment;
}
