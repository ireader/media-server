#include "file-reader.h"
#include "file-writer.h"
#include "mov-internal.h"
#include <assert.h>

struct mov_track_t* mov_track_find(const struct mov_t* mov, uint32_t track)
{
	size_t i;
	for (i = 0; i < mov->track_count; i++)
	{
		if (mov->tracks[i].tkhd.track_ID == track)
			return mov->tracks + i;
	}
	return NULL;
}

// 8.8.3 Track Extends Box (p69)
int mov_read_trex(struct mov_t* mov, const struct mov_box_t* box)
{
	uint32_t track_ID;
	struct mov_track_t* track;

	file_reader_rb32(mov->fp); /* version & flags */
	track_ID = file_reader_rb32(mov->fp); /* track_ID */

	track = mov_track_find(mov, track_ID);
	if (NULL == track)
		return -1;

	track->trex.default_sample_description_index = file_reader_rb32(mov->fp); /* default_sample_description_index */
	track->trex.default_sample_duration = file_reader_rb32(mov->fp); /* default_sample_duration */
	track->trex.default_sample_size = file_reader_rb32(mov->fp); /* default_sample_size */
	track->trex.default_sample_flags = file_reader_rb32(mov->fp); /* default_sample_flags */
	return file_reader_error(mov->fp); (void)box;
}

size_t mov_write_trex(const struct mov_t* mov)
{
	file_writer_wb32(mov->fp, 12 + 20); /* size */
	file_writer_write(mov->fp, "trex", 4);
	file_writer_wb32(mov->fp, 0); /* version & flags */
	file_writer_wb32(mov->fp, mov->track->tkhd.track_ID); /* track_ID */
	file_writer_wb32(mov->fp, 1); /* default_sample_description_index */
	file_writer_wb32(mov->fp, 0); /* default_sample_duration */
	file_writer_wb32(mov->fp, 0); /* default_sample_size */
	file_writer_wb32(mov->fp, 0); /* default_sample_flags */
	return 32;
}
