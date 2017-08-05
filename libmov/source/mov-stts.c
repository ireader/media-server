#include "file-reader.h"
#include "file-writer.h"
#include "mov-internal.h"
#include <errno.h>
#include <stdlib.h>
#include <assert.h>

// 8.6.1.2 Decoding Time to Sample Box (p47)
int mov_read_stts(struct mov_t* mov, const struct mov_box_t* box)
{
	uint32_t i, entry_count;
	struct mov_stbl_t* stbl = &mov->track->stbl;

	file_reader_r8(mov->fp); /* version */
	file_reader_rb24(mov->fp); /* flags */
	entry_count = file_reader_rb32(mov->fp);

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
		stbl->stts[i].sample_count = file_reader_rb32(mov->fp);
		stbl->stts[i].sample_delta = file_reader_rb32(mov->fp);
	}

	(void)box;
	return file_reader_error(mov->fp);
}

// 8.6.1.3 Composition Time to Sample Box (p47)
int mov_read_ctts(struct mov_t* mov, const struct mov_box_t* box)
{
	unsigned int version;
	uint32_t i, entry_count, sample_offset;
	struct mov_stbl_t* stbl = &mov->track->stbl;

	version = file_reader_r8(mov->fp); /* version */
	file_reader_rb24(mov->fp); /* flags */
	entry_count = file_reader_rb32(mov->fp);

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
		stbl->ctts[i].sample_count = file_reader_rb32(mov->fp);
		sample_offset = file_reader_rb32(mov->fp); // uint32_t sample_offset
		if (1 == version)
			stbl->ctts[i].sample_delta = (int32_t)sample_offset;
		else
			stbl->ctts[i].sample_delta = sample_offset;
	}

	(void)box;
	return file_reader_error(mov->fp);
}

size_t mov_write_stts(const struct mov_t* mov, uint32_t count)
{
	size_t size, i, j = 0;
	const struct mov_sample_t* sample;
	const struct mov_track_t* track = mov->track;

	size = 12/* full box */ + 4/* entry count */ + count * 8/* entry */;

	file_writer_wb32(mov->fp, size); /* size */
	file_writer_write(mov->fp, "stts", 4);
	file_writer_wb32(mov->fp, 0); /* version & flags */
	file_writer_wb32(mov->fp, count); /* entry count */

	for (i = 0; i < track->sample_count; i++)
	{
		sample = &track->samples[i];
		if (0 == sample->u.stts.count)
			continue;

		++j;
		file_writer_wb32(mov->fp, sample->u.stts.count); // count
		file_writer_wb32(mov->fp, sample->u.stts.duration); // duration * timescale / 1000
	}

	assert(j == count);
	return size;
}

size_t mov_write_ctts(const struct mov_t* mov, uint32_t count)
{
	size_t size, i, j = 0;
	const struct mov_sample_t* sample;
	const struct mov_track_t* track = mov->track;

	size = 12/* full box */ + 4/* entry count */ + count * 8/* entry */;

	file_writer_wb32(mov->fp, size); /* size */
	file_writer_write(mov->fp, "ctts", 4);
	file_writer_wb32(mov->fp, 0); /* version & flags */
	file_writer_wb32(mov->fp, count); /* entry count */

	for (i = 0; i < track->sample_count; i++)
	{
		sample = &track->samples[i];
		if (0 == sample->u.stts.count)
			continue;

		++j;
		file_writer_wb32(mov->fp, sample->u.stts.count); // count
		file_writer_wb32(mov->fp, sample->u.stts.duration); // duration
	}

	assert(j == count);
	return size;
}
