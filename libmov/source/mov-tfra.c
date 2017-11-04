#include "file-reader.h"
#include "file-writer.h"
#include "mov-internal.h"
#include <assert.h>

// 8.8.10 Track Fragment Random Access Box (p74)
int mov_read_tfra(struct mov_t* mov, const struct mov_box_t* box)
{
	unsigned int version;
	uint32_t track_ID;
	uint32_t length_size_of;
	uint32_t i, j, number_of_entry;
	uint32_t traf_number, trun_number, sample_number;
	struct mov_track_t* track;

	version = file_reader_r8(mov->fp); /* version */
	file_reader_rb24(mov->fp); /* flags */
	track_ID = file_reader_rb32(mov->fp); /* track_ID */

	track = mov_track_find(mov, track_ID);
	if (NULL == track)
		return -1;

	length_size_of = file_reader_rb32(mov->fp); /* length_size_of XXX */
	number_of_entry = file_reader_rb32(mov->fp); /* number_of_entry */
	for (i = 0; i < number_of_entry; i++)
	{
		if (1 == version)
		{
			file_reader_rb64(mov->fp); /* time */
			file_reader_rb64(mov->fp); /* moof_offset */
		}
		else
		{
			file_reader_rb32(mov->fp); /* time */
			file_reader_rb32(mov->fp); /* moof_offset */
		}

		for (traf_number = 0, j = 0; j < ((length_size_of >> 4) & 0x03) + 1; j++)
			traf_number = (traf_number << 8) | file_reader_r8(mov->fp); /* traf_number */

		for (trun_number = 0, j = 0; j < ((length_size_of >> 2) & 0x03) + 1; j++)
			trun_number = (trun_number << 8) | file_reader_r8(mov->fp); /* trun_number */

		for (sample_number = 0, j = 0; j < (length_size_of & 0x03) + 1; j++)
			sample_number = (sample_number << 8) | file_reader_r8(mov->fp); /* sample_number */
	}

	return file_reader_error(mov->fp); (void)box;
}

size_t mov_write_tfra(const struct mov_t* mov)
{
	size_t i, size;
	const struct mov_track_t* track = mov->track;

	size = 12/* full box */ + 12/* base */ + track->frag_count * 19/* index */;

	file_writer_wb32(mov->fp, size); /* size */
	file_writer_write(mov->fp, "tfra", 4);
	file_writer_w8(mov->fp, 1); /* version */
	file_writer_wb24(mov->fp, 0); /* flags */

	file_writer_wb32(mov->fp, track->tkhd.track_ID); /* track_ID */
	file_writer_wb32(mov->fp, 0); /* traf/trun/sample num */
	file_writer_wb32(mov->fp, track->frag_count); /* track_ID */

	for (i = 0; i < track->frag_count; i++)
	{
		file_writer_wb64(mov->fp, track->frags[i].time);
		file_writer_wb64(mov->fp, track->frags[i].offset); /* moof_offset */
		file_writer_w8(mov->fp, 1); /* traf number */
		file_writer_w8(mov->fp, 1); /* trun number */
		file_writer_w8(mov->fp, 1); /* sample number */
	}

	return size;
}
