#include "file-reader.h"
#include "file-writer.h"
#include "mov-internal.h"
#include <errno.h>
#include <stdlib.h>
#include <assert.h>

// 8.7.3.2 Sample Size Box (p57)
int mov_read_stsz(struct mov_t* mov, const struct mov_box_t* box)
{
	uint32_t i = 0, sample_size, sample_count;
	struct mov_track_t* track = mov->track;

	file_reader_r8(mov->fp); /* version */
	file_reader_rb24(mov->fp); /* flags */
	sample_size = file_reader_rb32(mov->fp);
	sample_count = file_reader_rb32(mov->fp);

	assert(0 == track->sample_count && NULL == track->samples); // duplicated STSZ atom
	if (track->sample_count < sample_count)
	{
		void* p = malloc(sizeof(struct mov_sample_t) * (sample_count + 1));
		if (NULL == p) return ENOMEM;
		track->samples = (struct mov_sample_t*)p;
	}
	track->sample_count = sample_count;

	if (0 == sample_size)
	{
		for (i = 0; i < sample_count; i++)
			track->samples[i].bytes = file_reader_rb32(mov->fp); // uint32_t entry_size
	}
	else
	{
		for (i = 0; i < sample_count; i++)
			track->samples[i].bytes = sample_size;
	}

	return file_reader_error(mov->fp);
}

// 8.7.3.3 Compact Sample Size Box (p57)
int mov_read_stz2(struct mov_t* mov, const struct mov_box_t* box)
{
	uint32_t i, v, field_size, sample_count;
	struct mov_track_t* track = mov->track;

	file_reader_r8(mov->fp); /* version */
	file_reader_rb24(mov->fp); /* flags */
	// unsigned int(24) reserved = 0;
	file_reader_rb24(mov->fp); /* reserved */
	field_size = file_reader_r8(mov->fp);
	sample_count = file_reader_rb32(mov->fp);

	assert(4 == field_size || 8 == field_size || 16 == field_size);
	assert(0 == track->sample_count && NULL == track->samples); // duplicated STSZ atom
	if (track->sample_count < sample_count)
	{
		void* p = malloc(sizeof(struct mov_sample_t) * (sample_count + 1));
		if (NULL == p) return ENOMEM;
		track->samples = (struct mov_sample_t*)p;
	}
	track->sample_count = sample_count;

	if (4 == field_size)
	{
		for (i = 0; i < sample_count/2; i++)
		{
			v = file_reader_r8(mov->fp);
			track->samples[i * 2].bytes = (v >> 4) & 0x0F;
			track->samples[i * 2 + 1].bytes = v & 0x0F;
		}
		if (sample_count % 2)
		{
			v = file_reader_r8(mov->fp);
			track->samples[i * 2].bytes = (v >> 4) & 0x0F;
		}
	}
	else if (8 == field_size)
	{
		for (i = 0; i < sample_count; i++)
			track->samples[i].bytes = file_reader_r8(mov->fp);
	}
	else if (16 == field_size)
	{
		for (i = 0; i < sample_count; i++)
			track->samples[i].bytes = file_reader_rb16(mov->fp);
	}
	else
	{
		i = 0;
		assert(0);
	}

	return file_reader_error(mov->fp);
}

size_t mov_write_stsz(const struct mov_t* mov)
{
	size_t size, i;
	const struct mov_track_t* track = mov->track;
	const struct mov_stbl_t* stbl = &mov->track->stbl;

	for(i = 1; i < track->sample_count; i++)
	{
		if(track->samples[i].bytes != track->samples[i-1].bytes)
			break;
	}

	size = 12/* full box */ + 8 + (i < track->sample_count ? 4 * track->sample_count : 0);
	file_writer_wb32(mov->fp, size); /* size */
	file_writer_write(mov->fp, "stsz", 4);
	file_writer_wb32(mov->fp, 0); /* version & flags */

	if(i < track->sample_count)
	{
		file_writer_wb32(mov->fp, 0);
		file_writer_wb32(mov->fp, stbl->stsc_count);
		for(i = 0; i < track->sample_count; i++)
			file_writer_wb32(mov->fp, track->samples[i].bytes);
	}
	else
	{
		file_writer_wb32(mov->fp, track->sample_count < 1 ? 0 : track->samples[0].bytes);
		file_writer_wb32(mov->fp, track->sample_count);
	}

	return size;
}
