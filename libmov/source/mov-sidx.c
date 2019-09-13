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

	(void)box;
	return mov_buffer_error(&mov->io);
}

size_t mov_write_sidx(const struct mov_t* mov, uint64_t offset)
{
    uint32_t duration;
    uint64_t earliest_presentation_time;
    const struct mov_track_t* track = mov->track;

    if (track->sample_count > 0)
    {
        earliest_presentation_time = track->samples[0].pts;
        duration = (uint32_t)(track->samples[track->sample_count - 1].dts - track->samples[0].dts);
    }
    else
    {
        duration = 0;
        earliest_presentation_time = 0;
    }

    mov_buffer_w32(&mov->io, 52); /* size */
    mov_buffer_write(&mov->io, "sidx", 4);
    mov_buffer_w8(&mov->io, 1); /* version */
    mov_buffer_w24(&mov->io, 0); /* flags */

    mov_buffer_w32(&mov->io, track->tkhd.track_ID); /* reference_ID */
    mov_buffer_w32(&mov->io, track->mdhd.timescale); /* timescale */
    mov_buffer_w64(&mov->io, earliest_presentation_time); /* earliest_presentation_time */
    mov_buffer_w64(&mov->io, offset); /* first_offset */
    mov_buffer_w16(&mov->io, 0); /* reserved */
    mov_buffer_w16(&mov->io, 1); /* reference_count */

    mov_buffer_w32(&mov->io, 0); /* reference_type & referenced_size */
    mov_buffer_w32(&mov->io, duration); /* subsegment_duration */
    mov_buffer_w32(&mov->io, (1U/*starts_with_SAP*/ << 31) | (1 /*SAP_type*/ << 24) | 0 /*SAP_delta_time*/);
    
    return 52;
}
