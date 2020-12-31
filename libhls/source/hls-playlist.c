#include "hls-parser.h"

int64_t hls_playlist_duration(const struct hls_playlist_t* playlist)
{
	size_t i;
	double duration;

	duration = 0.0;
	for (i = 0; i < playlist->count; i++)
	{
		duration += playlist->segments[i].duration;
	}

	return (int64_t)(duration * 1000);
}
