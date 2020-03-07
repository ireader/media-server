#include "mov-internal.h"
#include <errno.h>
#include <stdlib.h>
#include <assert.h>

// 8.6.6 Edit List Box (p53)
int mov_read_elst(struct mov_t* mov, const struct mov_box_t* box)
{
	uint32_t i, entry_count;
	uint32_t version;
	struct mov_track_t* track = mov->track;

	version = mov_buffer_r8(&mov->io); /* version */
	mov_buffer_r24(&mov->io); /* flags */
	entry_count = mov_buffer_r32(&mov->io);

	assert(0 == track->elst_count && NULL == track->elst);
	if (track->elst_count < entry_count)
	{
		void* p = realloc(track->elst, sizeof(struct mov_elst_t) * entry_count);
		if (NULL == p) return ENOMEM;
		track->elst = (struct mov_elst_t*)p;
	}
	track->elst_count = entry_count;

	for (i = 0; i < entry_count; i++)
	{
		if (1 == version)
		{
			track->elst[i].segment_duration = mov_buffer_r64(&mov->io);
			track->elst[i].media_time = (int64_t)mov_buffer_r64(&mov->io);
		}
		else
		{
			assert(0 == version);
			track->elst[i].segment_duration = mov_buffer_r32(&mov->io);
			track->elst[i].media_time = (int32_t)mov_buffer_r32(&mov->io);
		}
		track->elst[i].media_rate_integer = (int16_t)mov_buffer_r16(&mov->io);
		track->elst[i].media_rate_fraction = (int16_t)mov_buffer_r16(&mov->io);
	}

	(void)box;
	return mov_buffer_error(&mov->io);
}

size_t mov_write_elst(const struct mov_t* mov)
{
	uint32_t size;
	int64_t time;
	int64_t delay;
	uint8_t version;
	const struct mov_track_t* track = mov->track;

    assert(track->start_dts == track->samples[0].dts);
	version = track->tkhd.duration > UINT32_MAX ? 1 : 0;

    // in media time scale units, in composition time
	time = track->samples[0].pts - track->samples[0].dts;
    // in units of the timescale in the Movie Header Box
	delay = track->samples[0].pts * mov->mvhd.timescale / track->mdhd.timescale;
	if (delay > UINT32_MAX)
		version = 1;

	time = time < 0 ? 0 : time;
	size = 12/* full box */ + 4/* entry count */ + (delay > 0 ? 2 : 1) * (version ? 20 : 12);

	mov_buffer_w32(&mov->io, size); /* size */
	mov_buffer_write(&mov->io, "elst", 4);
	mov_buffer_w8(&mov->io, version); /* version */
	mov_buffer_w24(&mov->io, 0); /* flags */
	mov_buffer_w32(&mov->io, delay > 0 ? 2 : 1); /* entry count */

	if (delay > 0)
	{
		if (1 == version)
		{
			mov_buffer_w64(&mov->io, (uint64_t)delay); /* segment_duration */
			mov_buffer_w64(&mov->io, (uint64_t)-1); /* media_time */
		}
		else
		{
			mov_buffer_w32(&mov->io, (uint32_t)delay);
			mov_buffer_w32(&mov->io, (uint32_t)-1);
		}

		mov_buffer_w16(&mov->io, 1); /* media_rate_integer */
		mov_buffer_w16(&mov->io, 0); /* media_rate_fraction */
	}

	/* duration */
	if (version == 1) 
	{
		mov_buffer_w64(&mov->io, track->tkhd.duration);
		mov_buffer_w64(&mov->io, time);
	}
	else 
	{
		mov_buffer_w32(&mov->io, (uint32_t)track->tkhd.duration);
		mov_buffer_w32(&mov->io, (uint32_t)time);
	}
	mov_buffer_w16(&mov->io, 1); /* media_rate_integer */
	mov_buffer_w16(&mov->io, 0); /* media_rate_fraction */

	return size;
}

void mov_apply_elst(struct mov_track_t *track)
{
    size_t i;

    // edit list
    track->samples[0].dts = 0;
    track->samples[0].pts = 0;
    for (i = 0; i < track->elst_count; i++)
    {
        if (-1 == track->elst[i].media_time)
        {
            track->samples[0].dts = track->elst[i].segment_duration;
            track->samples[0].pts = track->samples[0].dts;
        }
    }
}

void mov_apply_elst_tfdt(struct mov_track_t *track)
{
    size_t i;

    for (i = 0; i < track->elst_count; i++)
    {
        if (-1 == track->elst[i].media_time)
        {
            track->tfdt_dts += track->elst[i].segment_duration;
        }
    }
}
