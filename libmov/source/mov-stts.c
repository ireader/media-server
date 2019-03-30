#include "mov-internal.h"
#include <errno.h>
#include <stdlib.h>
#include <assert.h>

// 8.6.1.2 Decoding Time to Sample Box (p47)
int mov_read_stts(struct mov_t* mov, const struct mov_box_t* box)
{
	uint32_t i, entry_count;
	struct mov_stbl_t* stbl = &mov->track->stbl;

	mov_buffer_r8(&mov->io); /* version */
	mov_buffer_r24(&mov->io); /* flags */
	entry_count = mov_buffer_r32(&mov->io);

	assert(0 == stbl->stts_count && NULL == stbl->stts); // duplicated STTS atom
	if (stbl->stts_count < entry_count)
	{
		void* p = realloc(stbl->stts, sizeof(struct mov_stts_t) * entry_count);
		if (NULL == p) return ENOMEM;
		stbl->stts = (struct mov_stts_t*)p;
	}
	stbl->stts_count = entry_count;

	for (i = 0; i < entry_count; i++)
	{
		stbl->stts[i].sample_count = mov_buffer_r32(&mov->io);
		stbl->stts[i].sample_delta = mov_buffer_r32(&mov->io);
	}

	(void)box;
	return mov_buffer_error(&mov->io);
}

// 8.6.1.3 Composition Time to Sample Box (p47)
int mov_read_ctts(struct mov_t* mov, const struct mov_box_t* box)
{
	uint32_t i, entry_count;
	struct mov_stbl_t* stbl = &mov->track->stbl;

	mov_buffer_r8(&mov->io); /* version */
	mov_buffer_r24(&mov->io); /* flags */
	entry_count = mov_buffer_r32(&mov->io);

	assert(0 == stbl->ctts_count && NULL == stbl->ctts); // duplicated CTTS atom
	if (stbl->ctts_count < entry_count)
	{
		void* p = realloc(stbl->ctts, sizeof(struct mov_stts_t) * entry_count);
		if (NULL == p) return ENOMEM;
		stbl->ctts = (struct mov_stts_t*)p;
	}
	stbl->ctts_count = entry_count;

	for (i = 0; i < entry_count; i++)
	{
		stbl->ctts[i].sample_count = mov_buffer_r32(&mov->io);
		stbl->ctts[i].sample_delta = mov_buffer_r32(&mov->io); // parse at int32_t
	}

	(void)box;
	return mov_buffer_error(&mov->io);
}

// 8.6.1.4 Composition to Decode Box (p53)
int mov_read_cslg(struct mov_t* mov, const struct mov_box_t* box)
{
	uint8_t version;
//	struct mov_stbl_t* stbl = &mov->track->stbl;

	version = (uint8_t)mov_buffer_r8(&mov->io); /* version */
	mov_buffer_r24(&mov->io); /* flags */

	if (0 == version)
	{
		(int32_t)mov_buffer_r32(&mov->io); /* compositionToDTSShift */
		(int32_t)mov_buffer_r32(&mov->io); /* leastDecodeToDisplayDelta */
		(int32_t)mov_buffer_r32(&mov->io); /* greatestDecodeToDisplayDelta */
		(int32_t)mov_buffer_r32(&mov->io); /* compositionStartTime */
		(int32_t)mov_buffer_r32(&mov->io); /* compositionEndTime */
	}
	else
	{
		(int64_t)mov_buffer_r64(&mov->io);
		(int64_t)mov_buffer_r64(&mov->io);
		(int64_t)mov_buffer_r64(&mov->io);
		(int64_t)mov_buffer_r64(&mov->io);
		(int64_t)mov_buffer_r64(&mov->io);
	}

	(void)box;
	return mov_buffer_error(&mov->io);
}

size_t mov_write_stts(const struct mov_t* mov, uint32_t count)
{
	size_t size, i;
	const struct mov_sample_t* sample;
	const struct mov_track_t* track = mov->track;

	size = 12/* full box */ + 4/* entry count */ + count * 8/* entry */;

	mov_buffer_w32(&mov->io, size); /* size */
	mov_buffer_write(&mov->io, "stts", 4);
	mov_buffer_w32(&mov->io, 0); /* version & flags */
	mov_buffer_w32(&mov->io, count); /* entry count */

	for (i = 0; i < track->sample_count; i++)
	{
		sample = &track->samples[i];
		if(0 == sample->first_chunk)
			continue;
		mov_buffer_w32(&mov->io, sample->first_chunk); // count
		mov_buffer_w32(&mov->io, sample->samples_per_chunk); // delta * timescale / 1000
	}

	return size;
}

size_t mov_write_ctts(const struct mov_t* mov, uint32_t count)
{
	size_t size, i;
	const struct mov_sample_t* sample;
	const struct mov_track_t* track = mov->track;

	size = 12/* full box */ + 4/* entry count */ + count * 8/* entry */;

	mov_buffer_w32(&mov->io, size); /* size */
	mov_buffer_write(&mov->io, "ctts", 4);
	mov_buffer_w8(&mov->io, 1); /* version */
	mov_buffer_w24(&mov->io, 0); /* flags */
	mov_buffer_w32(&mov->io, count); /* entry count */

	for (i = 0; i < track->sample_count; i++)
	{
		sample = &track->samples[i];
		if(0 == sample->first_chunk)
			continue;;
		mov_buffer_w32(&mov->io, sample->first_chunk); // count
		mov_buffer_w32(&mov->io, sample->samples_per_chunk); // offset * timescale / 1000
	}

	return size;
}

uint32_t mov_build_stts(struct mov_track_t* track)
{
    size_t i;
    uint32_t delta, count = 0;
    struct mov_sample_t* sample = NULL;

    for (i = 0; i < track->sample_count; i++)
    {
		assert(track->samples[i + 1].dts >= track->samples[i].dts || i + 1 == track->sample_count);
        delta = (uint32_t)(i + 1 < track->sample_count && track->samples[i + 1].dts > track->samples[i].dts ? track->samples[i + 1].dts - track->samples[i].dts : 1);
        if (NULL != sample && delta == sample->samples_per_chunk)
        {
            track->samples[i].first_chunk = 0;
            assert(sample->first_chunk > 0);
            ++sample->first_chunk; // compress
        }
        else
        {
            sample = &track->samples[i];
            sample->first_chunk = 1;
            sample->samples_per_chunk = delta;
            ++count;
        }
    }
    return count;
}

uint32_t mov_build_ctts(struct mov_track_t* track)
{
    size_t i;
    int32_t delta;
    uint32_t count = 0;
    struct mov_sample_t* sample = NULL;

    for (i = 0; i < track->sample_count; i++)
    {
        delta = (int32_t)(track->samples[i].pts - track->samples[i].dts);
        if (i > 0 && delta == (int32_t)sample->samples_per_chunk)
        {
            track->samples[i].first_chunk = 0;
            assert(sample->first_chunk > 0);
            ++sample->first_chunk; // compress
        }
        else
        {
            sample = &track->samples[i];
            sample->first_chunk = 1;
            sample->samples_per_chunk = delta;
            ++count;
        }
    }

    return count;
}

void mov_apply_stts(struct mov_track_t* track)
{
    size_t i, j, n;
    struct mov_stbl_t* stbl = &track->stbl;

    for (i = 0, n = 1; i < stbl->stts_count; i++)
    {
        for (j = 0; j < stbl->stts[i].sample_count; j++, n++)
        {
            track->samples[n].dts = track->samples[n - 1].dts + stbl->stts[i].sample_delta;
            track->samples[n].pts = track->samples[n].dts;
        }
    }
    assert(n - 1 == track->sample_count); // see more mov_read_stsz
}

void mov_apply_ctts(struct mov_track_t* track)
{
    size_t i, j, n;
    int32_t delta, dts_shift;
    struct mov_stbl_t* stbl = &track->stbl;

    // make sure pts >= dts
    dts_shift = 0;
    for (i = 0; i < stbl->ctts_count; i++)
    {
        delta = (int32_t)stbl->ctts[i].sample_delta;
        if (delta < 0 && dts_shift > delta && delta != -1 /* see more cslg box*/)
            dts_shift = delta;
    }
    assert(dts_shift <= 0);

    // sample cts/pts
    for (i = 0, n = 0; i < stbl->ctts_count; i++)
    {
        for (j = 0; j < stbl->ctts[i].sample_count; j++, n++)
            track->samples[n].pts += (int32_t)stbl->ctts[i].sample_delta - dts_shift; // always as int, fixed mp4box delta version error
    }
    assert(0 == stbl->ctts_count || n == track->sample_count);
}
