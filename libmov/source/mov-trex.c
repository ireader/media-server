#include "mov-internal.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct mov_track_t* mov_track_add(struct mov_t* mov)
{
    void* p;
    struct mov_track_t* track;
    p = realloc(mov->tracks, sizeof(struct mov_track_t) * (mov->track_count + 1));
    if (NULL == p) return NULL;

    mov->tracks = p;
    mov->track_count += 1;
    track = &mov->tracks[mov->track_count - 1];
    memset(track, 0, sizeof(struct mov_track_t));
    return track;
}

struct mov_track_t* mov_track_find(const struct mov_t* mov, uint32_t track)
{
	size_t i;
	for (i = 0; i < mov->track_count; i++)
	{
		if (mov->tracks[i].tkhd.track_ID == track)
			return mov->tracks + i;
	}
	return NULL;
}

// 8.8.3 Track Extends Box (p69)
int mov_read_trex(struct mov_t* mov, const struct mov_box_t* box)
{
	uint32_t track_ID;
	struct mov_track_t* track;

	mov_buffer_r32(&mov->io); /* version & flags */
	track_ID = mov_buffer_r32(&mov->io); /* track_ID */

	track = mov_track_find(mov, track_ID);
    if (NULL == track)
    {
        track = mov_track_add(mov);
        if (NULL == track)
            return -1;
        track->tkhd.track_ID = track_ID;
    }

	track->trex.default_sample_description_index = mov_buffer_r32(&mov->io); /* default_sample_description_index */
	track->trex.default_sample_duration = mov_buffer_r32(&mov->io); /* default_sample_duration */
	track->trex.default_sample_size = mov_buffer_r32(&mov->io); /* default_sample_size */
	track->trex.default_sample_flags = mov_buffer_r32(&mov->io); /* default_sample_flags */
	return mov_buffer_error(&mov->io); (void)box;
}

size_t mov_write_trex(const struct mov_t* mov)
{
	mov_buffer_w32(&mov->io, 12 + 20); /* size */
	mov_buffer_write(&mov->io, "trex", 4);
	mov_buffer_w32(&mov->io, 0); /* version & flags */
	mov_buffer_w32(&mov->io, mov->track->tkhd.track_ID); /* track_ID */
	mov_buffer_w32(&mov->io, 1); /* default_sample_description_index */
	mov_buffer_w32(&mov->io, 0); /* default_sample_duration */
	mov_buffer_w32(&mov->io, 0); /* default_sample_size */
	mov_buffer_w32(&mov->io, 0); /* default_sample_flags */
	return 32;
}
