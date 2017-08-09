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

	for (track = NULL, i = 0; i < mov->track_count; i++)
	{
		if (mov->tracks[i].tkhd.track_ID == track_ID)
		{
			track = mov->tracks + i;
			break;
		}
	}
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
