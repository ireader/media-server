#include "file-reader.h"
#include "file-writer.h"
#include "mov-internal.h"
#include <errno.h>
#include <stdlib.h>
#include <assert.h>

// 8.7.4 Sample To Chunk Box (p57)
/*
aligned(8) class SampleToChunkBox extends FullBox(¡®stsc¡¯, version = 0, 0) { 
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

	file_reader_r8(mov->fp); /* version */
	file_reader_rb24(mov->fp); /* flags */
	entry_count = file_reader_rb32(mov->fp);

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
		stbl->stsc[i].first_chunk = file_reader_rb32(mov->fp);
		stbl->stsc[i].samples_per_chunk = file_reader_rb32(mov->fp);
		stbl->stsc[i].sample_description_index = file_reader_rb32(mov->fp);
	}

	return file_reader_error(mov->fp);
}

size_t mov_write_stsc(const struct mov_t* mov, uint32_t count)
{
	size_t size, i, j = 0;
	const struct mov_sample_t* sample;
	const struct mov_track_t* track = mov->track;

	size = 12/* full box */ + 4/* entry count */ + count * 12/* entry */;

	file_writer_wb32(mov->fp, size); /* size */
	file_writer_write(mov->fp, "stsc", 4);
	file_writer_wb32(mov->fp, 0); /* version & flags */
	file_writer_wb32(mov->fp, count); /* entry count */

	for (i = 0; i < track->sample_count; i++)
	{
		sample = &track->samples[i];
		if (0 == sample->u.stsc.first_chunk)
			continue;

		++j;
		file_writer_wb32(mov->fp, sample->u.stsc.first_chunk);
		file_writer_wb32(mov->fp, sample->u.stsc.samples_per_chunk);
		file_writer_wb32(mov->fp, sample->u.stsc.sample_description_index);
	}

	assert(j == count);
	return size;
}
