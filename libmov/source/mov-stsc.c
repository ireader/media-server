#include "mov-internal.h"
#include <errno.h>
#include <stdlib.h>
#include <assert.h>

// 8.7.4 Sample To Chunk Box (p57)
/*
aligned(8) class SampleToChunkBox extends FullBox('stsc', version = 0, 0) { 
	unsigned int(32) entry_count; 
	for (i=1; i <= entry_count; i++) { 
		unsigned int(32) first_chunk; 
		unsigned int(32) samples_per_chunk; 
		unsigned int(32) sample_description_index; 
	} 
}
*/
int mov_read_stsc(struct mov_t* mov, const struct mov_box_t* box)
{
	uint32_t i, entry_count;
	struct mov_stbl_t* stbl = &mov->track->stbl;

	mov_buffer_r8(&mov->io); /* version */
	mov_buffer_r24(&mov->io); /* flags */
	entry_count = mov_buffer_r32(&mov->io);

	assert(0 == stbl->stsc_count && NULL == stbl->stsc); // duplicated STSC atom
	if (stbl->stsc_count < entry_count)
	{
		void* p = realloc(stbl->stsc, sizeof(struct mov_stsc_t) * (entry_count + 1/*stco count*/));
		if (NULL == p) return ENOMEM;
		stbl->stsc = (struct mov_stsc_t*)p;
	}
	stbl->stsc_count = entry_count;

	for (i = 0; i < entry_count; i++)
	{
		stbl->stsc[i].first_chunk = mov_buffer_r32(&mov->io);
		stbl->stsc[i].samples_per_chunk = mov_buffer_r32(&mov->io);
		stbl->stsc[i].sample_description_index = mov_buffer_r32(&mov->io);
	}

	(void)box;
	return mov_buffer_error(&mov->io);
}

size_t mov_write_stsc(const struct mov_t* mov)
{
	uint64_t offset;
	uint64_t offset2;
	uint32_t size, i, entry;
	const struct mov_sample_t* chunk = NULL;
	const struct mov_sample_t* sample = NULL;
	const struct mov_track_t* track = mov->track;

	size = 12/* full box */ + 4/* entry count */;

	offset = mov_buffer_tell(&mov->io);
	mov_buffer_w32(&mov->io, 0); /* size */
	mov_buffer_write(&mov->io, "stsc", 4);
	mov_buffer_w32(&mov->io, 0); /* version & flags */
	mov_buffer_w32(&mov->io, 0); /* entry count */

	for (i = 0, entry = 0; i < track->sample_count; i++)
	{
		sample = &track->samples[i];
		if (0 == sample->first_chunk || 
			(chunk && chunk->samples_per_chunk == sample->samples_per_chunk 
				&& chunk->sample_description_index == sample->sample_description_index))
			continue;

		++entry;
		chunk = sample;
		mov_buffer_w32(&mov->io, sample->first_chunk);
		mov_buffer_w32(&mov->io, sample->samples_per_chunk);
		mov_buffer_w32(&mov->io, sample->sample_description_index);
	}

	size += entry * 12/* entry size*/;
	offset2 = mov_buffer_tell(&mov->io);
	mov_buffer_seek(&mov->io, offset);
	mov_buffer_w32(&mov->io, size); /* size */
	mov_buffer_seek(&mov->io, offset + 12);
	mov_buffer_w32(&mov->io, entry); /* entry count */
	mov_buffer_seek(&mov->io, offset2);
	return size;
}
