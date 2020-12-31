#include "mkv-internal.h"

int mkv_track_free(struct mkv_track_t* track)
{
	if (track->name)
		free(track->name);
	if (track->lang)
		free(track->lang);
	if (track->codec_extra.ptr)
		free(track->codec_extra.ptr);
	return 0;
}

struct mkv_track_t* mkv_track_find(struct mkv_t* mkv, int id)
{
	int i;
	struct mkv_track_t* track;

	for (i = 0; i < mkv->track_count; i++)
	{
		track = &mkv->tracks[i];
		if (track->id == id)
			return track;
	}
	return NULL;
}
