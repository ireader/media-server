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
