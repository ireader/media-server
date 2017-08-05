#include "file-reader.h"
#include "file-writer.h"
#include "mov-internal.h"
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

// 8.6.2 Sync Sample Box (p50)
int mov_read_stss(struct mov_t* mov, const struct mov_box_t* box)
{
	uint32_t i, entry_count;
	struct mov_stbl_t* stbl = &mov->track->stbl;

	file_reader_r8(mov->fp); /* version */
	file_reader_rb24(mov->fp); /* flags */
	entry_count = file_reader_rb32(mov->fp);

	assert(0 == stbl->stss_count && NULL == stbl->stss);
	if (stbl->stss_count < entry_count)
	{
		void* p = realloc(stbl->stss, sizeof(stbl->stss[0]) * entry_count);
		if (NULL == p) return ENOMEM;
		stbl->stss = p;
	}
	stbl->stss_count = entry_count;

	for (i = 0; i < entry_count; i++)
		stbl->stss[i] = file_reader_rb32(mov->fp); // uint32_t sample_number

	(void)box;
	return file_reader_error(mov->fp);
}

size_t mov_write_stss(const struct mov_t* mov)
{
	uint64_t offset;
	uint64_t offset2;
	size_t size, i, j;
	const struct mov_sample_t* sample;
	const struct mov_track_t* track = mov->track;

	size = 12/* full box */ + 4/* entry count */;

	offset = file_writer_tell(mov->fp);
	file_writer_wb32(mov->fp, 0); /* size */
	file_writer_write(mov->fp, "stss", 4);
	file_writer_wb32(mov->fp, 0); /* version & flags */
	file_writer_wb32(mov->fp, 0); /* entry count */

	for (i = 0, j = 0; i < track->sample_count; i++)
	{
		sample = &track->samples[i];
		if (sample->flags & MOV_AV_FLAG_KEYFREAME)
		{
			++j;
			file_writer_wb32(mov->fp, i + 1);
		}
	}

	size += j * 4/* entry */;
	offset2 = file_writer_tell(mov->fp);
	file_writer_seek(mov->fp, offset);
	file_writer_wb32(mov->fp, size); /* size */
	file_writer_seek(mov->fp, offset + 12);
	file_writer_wb32(mov->fp, j); /* entry count */
	file_writer_seek(mov->fp, offset2);
	return size;
}
