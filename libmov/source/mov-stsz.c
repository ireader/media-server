#include "mov-internal.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// 8.7.3.2 Sample Size Box (p57)
int mov_read_stsz(struct mov_t* mov, const struct mov_box_t* box)
{
	uint32_t i = 0, sample_size, sample_count;
	struct mov_track_t* track = mov->track;

	mov_buffer_r8(&mov->io); /* version */
	mov_buffer_r24(&mov->io); /* flags */
	sample_size = mov_buffer_r32(&mov->io);
	sample_count = mov_buffer_r32(&mov->io);

	assert(0 == track->sample_count && NULL == track->samples); // duplicated STSZ atom
	if (track->sample_count < sample_count)
	{
		void* p = realloc(track->samples, sizeof(struct mov_sample_t) * (sample_count + 1));
		if (NULL == p) return ENOMEM;
		track->samples = (struct mov_sample_t*)p;
		memset(track->samples, 0, sizeof(struct mov_sample_t) * (sample_count + 1));
	}
	track->sample_count = sample_count;

	if (0 == sample_size)
	{
		for (i = 0; i < sample_count; i++)
			track->samples[i].bytes = mov_buffer_r32(&mov->io); // uint32_t entry_size
	}
	else
	{
		for (i = 0; i < sample_count; i++)
			track->samples[i].bytes = sample_size;
	}

	(void)box;
	return mov_buffer_error(&mov->io);
}

// 8.7.3.3 Compact Sample Size Box (p57)
int mov_read_stz2(struct mov_t* mov, const struct mov_box_t* box)
{
	uint32_t i, v, field_size, sample_count;
	struct mov_track_t* track = mov->track;

	mov_buffer_r8(&mov->io); /* version */
	mov_buffer_r24(&mov->io); /* flags */
	// unsigned int(24) reserved = 0;
	mov_buffer_r24(&mov->io); /* reserved */
	field_size = mov_buffer_r8(&mov->io);
	sample_count = mov_buffer_r32(&mov->io);

	assert(4 == field_size || 8 == field_size || 16 == field_size);
	assert(0 == track->sample_count && NULL == track->samples); // duplicated STSZ atom
	if (track->sample_count < sample_count)
	{
		void* p = realloc(track->samples, sizeof(struct mov_sample_t) * (sample_count + 1));
		if (NULL == p) return ENOMEM;
		track->samples = (struct mov_sample_t*)p;
		memset(track->samples, 0, sizeof(struct mov_sample_t) * (sample_count + 1));
	}
	track->sample_count = sample_count;

	if (4 == field_size)
	{
		for (i = 0; i < sample_count/2; i++)
		{
			v = mov_buffer_r8(&mov->io);
			track->samples[i * 2].bytes = (v >> 4) & 0x0F;
			track->samples[i * 2 + 1].bytes = v & 0x0F;
		}
		if (sample_count % 2)
		{
			v = mov_buffer_r8(&mov->io);
			track->samples[i * 2].bytes = (v >> 4) & 0x0F;
		}
	}
	else if (8 == field_size)
	{
		for (i = 0; i < sample_count; i++)
			track->samples[i].bytes = mov_buffer_r8(&mov->io);
	}
	else if (16 == field_size)
	{
		for (i = 0; i < sample_count; i++)
			track->samples[i].bytes = mov_buffer_r16(&mov->io);
	}
	else
	{
		i = 0;
		assert(0);
	}

	(void)box;
	return mov_buffer_error(&mov->io);
}

size_t mov_write_stsz(const struct mov_t* mov)
{
	size_t size, i;
	const struct mov_track_t* track = mov->track;

	for(i = 1; i < track->sample_count; i++)
	{
		if(track->samples[i].bytes != track->samples[i-1].bytes)
			break;
	}

	size = 12/* full box */ + 8 + (i < track->sample_count ? 4 * track->sample_count : 0);
	mov_buffer_w32(&mov->io, size); /* size */
	mov_buffer_write(&mov->io, "stsz", 4);
	mov_buffer_w32(&mov->io, 0); /* version & flags */

	if(i < track->sample_count)
	{
		mov_buffer_w32(&mov->io, 0);
		mov_buffer_w32(&mov->io, track->sample_count);
		for(i = 0; i < track->sample_count; i++)
			mov_buffer_w32(&mov->io, track->samples[i].bytes);
	}
	else
	{
		mov_buffer_w32(&mov->io, track->sample_count < 1 ? 0 : track->samples[0].bytes);
		mov_buffer_w32(&mov->io, track->sample_count);
	}

	return size;
}
