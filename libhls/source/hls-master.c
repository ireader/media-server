#include "hls-parser.h"
#include "hls-string.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

// Typically, closed-caption [CEA608] media is carried in the video stream.  
// Therefore, an EXT-X-MEDIA tag with TYPE of CLOSED-CAPTIONS does not specify 
// a Rendition; the closed-caption media is present in the Media Segments of every video Rendition.

int hls_master_rendition(const struct hls_master_t* master, int variant, int type, const char* name)
{
	size_t i;
	int is_default;
	int autoselect;
	struct hls_media_t* m;
	struct hls_variant_t* v;
	const char* groups[4];
	const char* renditions[] = { "AUDIO", "VIDEO", "SUBTITLES", "CLOSED-CAPTIONS", };

	if (variant < 0 || variant >= (int)master->variant_count
		|| type < HLS_MEDIA_AUDIO || type > HLS_MEDIA_CLOSED_CAPTIONS)
		return -ENOENT;

	type -= HLS_MEDIA_AUDIO;
	v = &master->variants[variant];
	groups[0] = v->audio ? v->audio : "";
	groups[1] = v->video ? v->video : "";
	groups[2] = v->subtitle ? v->subtitle : "";
	groups[3] = v->closed_captions ? v->closed_captions : "";
	if (!*groups[type])
		return -1; // don't have alternative renditions

	is_default = -1;
	autoselect = -1;
	for (i = 0; i < master->media_count; i++)
	{
		m = &master->medias[i];
		if (0 != strcasecmp(renditions[type], m->type))
			continue;

		if (!m->group_id || 0 != strcmp(groups[type], m->group_id))
			continue;

		if (name)
		{
			if(m->name && 0 == strcmp(name, m->name))
				return (int)i;
		}
		else
		{
			if (m->is_default)
				is_default = (int)i;
			else if(m->autoselect)
				autoselect = (int)i;
		}
	}

	return is_default > 0 ? is_default : (autoselect ? autoselect : -1);
}

int hls_master_best_variant(const struct hls_master_t* master)
{
	int best;
	size_t i;
	struct hls_variant_t* v;

	best = -1;
	for (i = 0; i < master->variant_count; i++)
	{
		v = &master->variants[i];
		if (-1 == best || v->bandwidth > master->variants[best].bandwidth)
			best = (int)i;
	}

	return best;
}
