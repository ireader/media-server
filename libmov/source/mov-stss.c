#include "mov-internal.h"
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

// 8.6.2 Sync Sample Box (p50)
int mov_read_stss(struct mov_t* mov, const struct mov_box_t* box)
{
	uint32_t i, entry_count;
	struct mov_stbl_t* stbl = &mov->track->stbl;

	mov_buffer_r8(&mov->io); /* version */
	mov_buffer_r24(&mov->io); /* flags */
	entry_count = mov_buffer_r32(&mov->io);

	assert(0 == stbl->stss_count && NULL == stbl->stss);
	if (stbl->stss_count < entry_count)
	{
		void* p = realloc(stbl->stss, sizeof(stbl->stss[0]) * entry_count);
		if (NULL == p) return ENOMEM;
		stbl->stss = p;
	}
	stbl->stss_count = entry_count;

	for (i = 0; i < entry_count; i++)
		stbl->stss[i] = mov_buffer_r32(&mov->io); // uint32_t sample_number

	(void)box;
	return mov_buffer_error(&mov->io);
}

size_t mov_write_stss(const struct mov_t* mov)
{
	uint64_t offset;
	uint64_t offset2;
	size_t size, i, j;
	const struct mov_sample_t* sample;
	const struct mov_track_t* track = mov->track;

	size = 12/* full box */ + 4/* entry count */;

	offset = mov_buffer_tell(&mov->io);
	mov_buffer_w32(&mov->io, 0); /* size */
	mov_buffer_write(&mov->io, "stss", 4);
	mov_buffer_w32(&mov->io, 0); /* version & flags */
	mov_buffer_w32(&mov->io, 0); /* entry count */

	for (i = 0, j = 0; i < track->sample_count; i++)
	{
		sample = &track->samples[i];
		if (sample->flags & MOV_AV_FLAG_KEYFREAME)
		{
			++j;
			mov_buffer_w32(&mov->io, i + 1); // start from 1
		}
	}

	size += j * 4/* entry */;
	offset2 = mov_buffer_tell(&mov->io);
	mov_buffer_seek(&mov->io, offset);
	mov_buffer_w32(&mov->io, size); /* size */
	mov_buffer_seek(&mov->io, offset + 12);
	mov_buffer_w32(&mov->io, j); /* entry count */
	mov_buffer_seek(&mov->io, offset2);
	return size;
}

void mov_apply_stss(struct mov_track_t* track)
{
	size_t i, j;
	struct mov_stbl_t* stbl = &track->stbl;

	for (i = 0; i < stbl->stss_count; i++)
	{
		j = stbl->stss[i]; // start from 1
		if (j > 0 && j <= track->sample_count);
			track->samples[j - 1].flags |= MOV_AV_FLAG_KEYFREAME;
	}
}
