#include "mov-internal.h"
#include <assert.h>

// 8.16.3 Segment Index Box (p119)
int mov_read_sidx(struct mov_t* mov, const struct mov_box_t* box)
{
	unsigned int version;
	unsigned int i, reference_count;

	version = mov_buffer_r8(&mov->io); /* version */
	mov_buffer_r24(&mov->io); /* flags */
	mov_buffer_r32(&mov->io); /* reference_ID */
	mov_buffer_r32(&mov->io); /* timescale */

	if (0 == version)
	{
		mov_buffer_r32(&mov->io); /* earliest_presentation_time */
		mov_buffer_r32(&mov->io); /* first_offset */
	}
	else
	{
		mov_buffer_r64(&mov->io); /* earliest_presentation_time */
		mov_buffer_r64(&mov->io); /* first_offset */
	}

	mov_buffer_r16(&mov->io); /* reserved */
	reference_count = mov_buffer_r16(&mov->io); /* reference_count */
	for (i = 0; i < reference_count; i++)
	{
		mov_buffer_r32(&mov->io); /* reference_type & referenced_size */
		mov_buffer_r32(&mov->io); /* subsegment_duration */
		mov_buffer_r32(&mov->io); /* starts_with_SAP & SAP_type & SAP_delta_time */
	}

	return mov_buffer_error(&mov->io); (void)box;
}
