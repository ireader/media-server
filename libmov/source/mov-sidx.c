#include "file-reader.h"
#include "file-writer.h"
#include "mov-internal.h"
#include <assert.h>

// 8.16.3 Segment Index Box (p119)
int mov_read_sidx(struct mov_t* mov, const struct mov_box_t* box)
{
	unsigned int version;
	unsigned int i, reference_count;
	struct mov_track_t* track;

	track = mov->track;

	version = file_reader_r8(mov->fp); /* version */
	file_reader_rb24(mov->fp); /* flags */
	file_reader_rb32(mov->fp); /* reference_ID */
	file_reader_rb32(mov->fp); /* timescale */

	if (0 == version)
	{
		file_reader_rb32(mov->fp); /* earliest_presentation_time */
		file_reader_rb32(mov->fp); /* first_offset */
	}
	else
	{
		file_reader_rb64(mov->fp); /* earliest_presentation_time */
		file_reader_rb64(mov->fp); /* first_offset */
	}

	file_reader_rb16(mov->fp); /* reserved */
	reference_count = file_reader_rb16(mov->fp); /* reference_count */
	for (i = 0; i < reference_count; i++)
	{
		file_reader_rb32(mov->fp); /* reference_type & referenced_size */
		file_reader_rb32(mov->fp); /* subsegment_duration */
		file_reader_rb32(mov->fp); /* starts_with_SAP & SAP_type & SAP_delta_time */
	}

	return file_reader_error(mov->fp); (void)box;
}
