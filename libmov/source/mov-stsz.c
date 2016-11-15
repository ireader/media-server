#include "file-reader.h"
#include "mov-internal.h"
#include <errno.h>
#include <stdlib.h>
#include <assert.h>

// 8.7.3.2 Sample Size Box (p57)
int mov_read_stsz(struct mov_reader_t* mov, const struct mov_box_t* box)
{
	struct mov_track_t* track;
	uint32_t i = 0, sample_size, sample_count;
	file_reader_r8(mov->fp); /* version */
	file_reader_rb24(mov->fp); /* flags */
	sample_size = file_reader_rb32(mov->fp);
	sample_count = file_reader_rb32(mov->fp);

	assert(mov->track);
	track = mov->track;
	if (track->stsz)
	{
		assert(0);
		free(track->stsz); // duplicated STSZ atom
	}
	track->stsz_count = 0;
	track->stsz = malloc(sizeof(*track->stsz) * sample_count);
	if (NULL == track->stsz)
		return ENOMEM;

	if (0 == sample_size)
	{
		for (i = 0; i < sample_count; i++)
			track->stsz[i] = file_reader_rb32(mov->fp); // uint32_t entry_size
	}
	else
	{
		for (i = 0; i < sample_count; i++)
			track->stsz[i] = sample_size;
	}

	track->stsz_count = i;
	return file_reader_error(mov->fp);
}

// 8.7.3.3 Compact Sample Size Box (p57)
int mov_read_stz2(struct mov_reader_t* mov, const struct mov_box_t* box)
{
	struct mov_track_t* track;
	uint32_t i, v, field_size, sample_count;
	file_reader_r8(mov->fp); /* version */
	file_reader_rb24(mov->fp); /* flags */
	// unsigned int(24) reserved = 0;
	file_reader_rb24(mov->fp); /* reserved */
	field_size = file_reader_r8(mov->fp);
	sample_count = file_reader_rb32(mov->fp);

	assert(4 == field_size || 8 == field_size || 16 == field_size);
	assert(mov->track);
	track = mov->track;
	if (track->stsz)
	{
		assert(0);
		free(track->stsz); // duplicated STSZ atom
	}
	track->stsz_count = 0;
	track->stsz = malloc(sizeof(*track->stsz) * sample_count);
	if (NULL == track->stsz)
		return ENOMEM;

	if (4 == field_size)
	{
		for (i = 0; i < sample_count/2; i++)
		{
			v = file_reader_r8(mov->fp);
			track->stsz[i * 2] = (v >> 4) & 0x0F;
			track->stsz[i * 2 + 1] = v & 0x0F;
		}
		if (sample_count % 2)
		{
			v = file_reader_r8(mov->fp);
			track->stsz[i * 2] = (v >> 4) & 0x0F;
		}
	}
	else if (8 == field_size)
	{
		for (i = 0; i < sample_count; i++)
			track->stsz[i] = file_reader_r8(mov->fp);
	}
	else if (16 == field_size)
	{
		for (i = 0; i < sample_count; i++)
			track->stsz[i] = file_reader_rb16(mov->fp);
	}
	else
	{
		i = 0;
		assert(0);
	}

	track->stsz_count = i;
	return file_reader_error(mov->fp);
}
