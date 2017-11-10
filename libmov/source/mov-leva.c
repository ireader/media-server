#include "file-reader.h"
#include "file-writer.h"
#include "mov-internal.h"
#include <assert.h>

// 8.8.13 Level Assignment Box (p77)
int mov_read_leva(struct mov_t* mov, const struct mov_box_t* box)
{
	unsigned int i, level_count;
	unsigned int assignment_type;

	file_reader_rb32(mov->fp); /* version & flags */
	level_count = file_reader_r8(mov->fp); /* level_count */
	for (i = 0; i < level_count; i++)
	{
		file_reader_rb32(mov->fp); /* track_id */
		assignment_type = file_reader_r8(mov->fp); /* padding_flag & assignment_type */
		assignment_type &= 0x7F; // 7-bits

		if (0 == assignment_type)
		{
			file_reader_rb32(mov->fp); /* grouping_type */
		}
		else if (1 == assignment_type)
		{
			file_reader_rb32(mov->fp); /* grouping_type */
			file_reader_rb32(mov->fp); /* grouping_type_parameter */
		}
		else if (4 == assignment_type)
		{
			file_reader_rb32(mov->fp); /* sub_track_id */
		}
	}

	return file_reader_error(mov->fp); (void)box;
}
