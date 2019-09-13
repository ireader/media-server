#include "mov-internal.h"
#include <assert.h>

// 8.8.13 Level Assignment Box (p77)
int mov_read_leva(struct mov_t* mov, const struct mov_box_t* box)
{
	unsigned int i, level_count;
	unsigned int assignment_type;

	mov_buffer_r32(&mov->io); /* version & flags */
	level_count = mov_buffer_r8(&mov->io); /* level_count */
	for (i = 0; i < level_count; i++)
	{
		mov_buffer_r32(&mov->io); /* track_id */
		assignment_type = mov_buffer_r8(&mov->io); /* padding_flag & assignment_type */
		assignment_type &= 0x7F; // 7-bits

		if (0 == assignment_type)
		{
			mov_buffer_r32(&mov->io); /* grouping_type */
		}
		else if (1 == assignment_type)
		{
			mov_buffer_r32(&mov->io); /* grouping_type */
			mov_buffer_r32(&mov->io); /* grouping_type_parameter */
		}
		else if (4 == assignment_type)
		{
			mov_buffer_r32(&mov->io); /* sub_track_id */
		}
	}

	(void)box;
	return mov_buffer_error(&mov->io);
}
