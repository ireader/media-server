#include "mov-internal.h"
#include <assert.h>

// 8.8.7 Track Fragment Header Box (p71)
int mov_read_tfhd(struct mov_t* mov, const struct mov_box_t* box)
{
	uint32_t flags;
	uint32_t track_ID;

	mov_buffer_r8(&mov->io); /* version */
	flags = mov_buffer_r24(&mov->io); /* flags */
	track_ID = mov_buffer_r32(&mov->io); /* track_ID */
	
	mov->track = mov_find_track(mov, track_ID);
	if (NULL == mov->track)
		return -1;

	mov->track->tfhd.flags = flags;

	if (MOV_TFHD_FLAG_BASE_DATA_OFFSET & flags)
		mov->track->tfhd.base_data_offset = mov_buffer_r64(&mov->io); /* base_data_offset*/
	else if(MOV_TFHD_FLAG_DEFAULT_BASE_IS_MOOF & flags)
		mov->track->tfhd.base_data_offset = mov->moof_offset; /* default©\base©\is©\moof */
	else
		mov->track->tfhd.base_data_offset = mov->implicit_offset;

	if (MOV_TFHD_FLAG_SAMPLE_DESCRIPTION_INDEX & flags)
		mov->track->tfhd.sample_description_index = mov_buffer_r32(&mov->io); /* sample_description_index*/
	else
		mov->track->tfhd.sample_description_index = mov->track->trex.default_sample_description_index;

	if (MOV_TFHD_FLAG_DEFAULT_DURATION & flags)
		mov->track->tfhd.default_sample_duration = mov_buffer_r32(&mov->io); /* default_sample_duration*/
	else
		mov->track->tfhd.default_sample_duration = mov->track->trex.default_sample_duration;

	if (MOV_TFHD_FLAG_DEFAULT_SIZE & flags)
		mov->track->tfhd.default_sample_size = mov_buffer_r32(&mov->io); /* default_sample_size*/
	else
		mov->track->tfhd.default_sample_size = mov->track->trex.default_sample_size;

	if (MOV_TFHD_FLAG_DEFAULT_FLAGS & flags)
		mov->track->tfhd.default_sample_flags = mov_buffer_r32(&mov->io); /* default_sample_flags*/
	else
		mov->track->tfhd.default_sample_flags = mov->track->trex.default_sample_flags;

	if (MOV_TFHD_FLAG_DURATION_IS_EMPTY & flags)
		(void)box; /* duration©\is©\empty*/
	return mov_buffer_error(&mov->io);
}

size_t mov_write_tfhd(const struct mov_t* mov)
{
	size_t size;
	uint64_t offset;

	size = 12 + 4 /* track_ID */;

	offset = mov_buffer_tell(&mov->io);
	mov_buffer_w32(&mov->io, 0); /* size */
	mov_buffer_write(&mov->io, "tfhd", 4);
	mov_buffer_w8(&mov->io, 0); /* version */
	mov_buffer_w24(&mov->io, mov->track->tfhd.flags); /* flags */
	mov_buffer_w32(&mov->io, mov->track->tkhd.track_ID); /* track_ID */

	if (MOV_TFHD_FLAG_BASE_DATA_OFFSET & mov->track->tfhd.flags)
	{
		mov_buffer_w64(&mov->io, mov->track->tfhd.base_data_offset); /* base_data_offset*/
		size += 8;
	}

	if (MOV_TFHD_FLAG_SAMPLE_DESCRIPTION_INDEX & mov->track->tfhd.flags)
	{
		mov_buffer_w32(&mov->io, mov->track->stsd.entries[0].data_reference_index); /* sample_description_index*/
		size += 4;
	}

	if (MOV_TFHD_FLAG_DEFAULT_DURATION & mov->track->tfhd.flags)
	{
		mov_buffer_w32(&mov->io, mov->track->tfhd.default_sample_duration); /* default_sample_duration*/
		size += 4;
	}

	if (MOV_TFHD_FLAG_DEFAULT_SIZE & mov->track->tfhd.flags)
	{
		mov_buffer_w32(&mov->io, mov->track->tfhd.default_sample_size); /* default_sample_size*/
		size += 4;
	}

	if (MOV_TFHD_FLAG_DEFAULT_FLAGS & mov->track->tfhd.flags)
	{
		mov_buffer_w32(&mov->io, mov->track->tfhd.default_sample_flags); /* default_sample_flags*/
		size += 4;
	}

	mov_write_size(mov, offset, size);
	return size;
}
