#include "dash-parser.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int dash_mime_type_to_media_type(const char* mime)
{
	switch (mime[0])
	{
	case 't':
		return 0 == strncmp(mime, "text/", 5) ? DASH_MEDIA_TEXT : DASH_MEDIA_UNKNOWN;
	case 'i':
		return 0 == strncmp(mime, "image/", 6) ? DASH_MEDIA_IMAGE : DASH_MEDIA_UNKNOWN;
	case 'a':
		if (0 == strncmp(mime, "audio/", 6))
			return DASH_MEDIA_AUDIO;
		else if (0 == strncmp(mime, "application/", 12))
			return DASH_MEDIA_APPLICATION;
		else
			return DASH_MEDIA_UNKNOWN;
	case 'v':
		return 0 == strncmp(mime, "video/", 6) ? DASH_MEDIA_VIDEO : DASH_MEDIA_UNKNOWN;
	case 'f':
		return 0 == strncmp(mime, "font/", 5) ? DASH_MEDIA_FONT : DASH_MEDIA_UNKNOWN;
	default:
		return DASH_MEDIA_UNKNOWN;
	}
}

int dash_adaptation_set_media_type(const struct dash_adaptation_set_t* set)
{
	const struct dash_representation_base_t* base;

	base = &set->base;
	if (base->mime_type && *base->mime_type)
		return dash_mime_type_to_media_type(base->mime_type);
	else if (set->content_type && *set->content_type)
		return dash_mime_type_to_media_type(set->content_type);

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

int dash_adaptation_set_best_representation(const struct dash_adaptation_set_t* set)
{
	int best;
	size_t i;
	const struct dash_representation_t* v;

	best = -1;
	for (i = 0; i < set->representation_count; i++)
	{
		v = &set->representations[i];
		// qualityRanking: specifies a quality ranking of the Representation 
		// relative to other Representations in the same Adaptation Set. 
		// Lower values represent higher quality content.
		if (-1 == best
			|| v->base.selection_priority > set->representations[best].base.selection_priority
			|| v->quality_ranking < set->representations[best].quality_ranking
			|| v->bandwidth > set->representations[best].bandwidth)
			best = (int)i;
	}

	return best;
}
