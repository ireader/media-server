#include "file-reader.h"
#include "file-writer.h"
#include "mov-internal.h"
#include <assert.h>

// 8.8.7 Track Fragment Header Box (p71)
int mov_read_tfhd(struct mov_t* mov, const struct mov_box_t* box)
{
	uint32_t flags;
	uint32_t track_ID;

	file_reader_r8(mov->fp); /* version */
	flags = file_reader_rb24(mov->fp); /* flags */
	track_ID = file_reader_rb32(mov->fp); /* track_ID */
	
	mov->track = mov_track_find(mov, track_ID);
	if (NULL == mov->track)
		return -1;

	mov->track->tfhd.flags = flags;

	if (MOV_TFHD_FLAG_BASE_DATA_OFFSET & flags)
		mov->track->tfhd.base_data_offset = file_reader_rb64(mov->fp); /* base_data_offset*/
	else if(MOV_TFHD_FLAG_DEFAULT_BASE_IS_MOOF & flags)
		mov->track->tfhd.base_data_offset = mov->moof_offset; /* default©\base©\is©\moof */
	else
		mov->track->tfhd.base_data_offset = mov->track->sample_count > 0 ? (mov->track->samples[mov->track->sample_count - 1].offset + mov->track->samples[mov->track->sample_count - 1].bytes) : 0;

	if (MOV_TFHD_FLAG_SAMPLE_DESCRIPTION_INDEX & flags)
		mov->track->tfhd.sample_description_index = file_reader_rb32(mov->fp); /* sample_description_index*/
	else
		mov->track->tfhd.sample_description_index = mov->track->trex.default_sample_description_index;

	if (MOV_TFHD_FLAG_DEFAULT_DURATION & flags)
		mov->track->tfhd.default_sample_duration = file_reader_rb32(mov->fp); /* default_sample_duration*/
	else
		mov->track->tfhd.default_sample_duration = mov->track->trex.default_sample_duration;

	if (MOV_TFHD_FLAG_DEFAULT_SIZE & flags)
		mov->track->tfhd.default_sample_size = file_reader_rb32(mov->fp); /* default_sample_size*/
	else
		mov->track->tfhd.default_sample_size = mov->track->trex.default_sample_size;

	if (MOV_TFHD_FLAG_DEFAULT_FLAGS & flags)
		mov->track->tfhd.default_sample_flags = file_reader_rb32(mov->fp); /* default_sample_flags*/
	else
		mov->track->tfhd.default_sample_flags = mov->track->trex.default_sample_flags;

	if (MOV_TFHD_FLAG_DURATION_IS_EMPTY & flags)
		; /* duration©\is©\empty*/
	return file_reader_error(mov->fp); (void)box;
}

size_t mov_write_tfhd(const struct mov_t* mov)
{
	size_t size;
	uint64_t offset;

	size = 12 + 4 /* track_ID */;

	offset = file_writer_tell(mov->fp);
	file_writer_wb32(mov->fp, 0); /* size */
	file_writer_write(mov->fp, "tfhd", 4);
	file_writer_w8(mov->fp, 0); /* version */
	file_writer_wb24(mov->fp, mov->track->tfhd.flags); /* flags */
	file_writer_wb32(mov->fp, mov->track->tkhd.track_ID); /* track_ID */

	if (MOV_TFHD_FLAG_BASE_DATA_OFFSET & mov->track->tfhd.flags)
	{
		file_writer_wb64(mov->fp, mov->track->tfhd.base_data_offset); /* base_data_offset*/
		size += 8;
	}

	if (MOV_TFHD_FLAG_SAMPLE_DESCRIPTION_INDEX & mov->track->tfhd.flags)
	{
		file_writer_wb32(mov->fp, mov->track->stsd[0].data_reference_index); /* sample_description_index*/
		size += 4;
	}

	if (MOV_TFHD_FLAG_DEFAULT_DURATION & mov->track->tfhd.flags)
	{
		file_writer_wb32(mov->fp, mov->track->tfhd.default_sample_duration); /* default_sample_duration*/
		size += 4;
	}

	if (MOV_TFHD_FLAG_DEFAULT_SIZE & mov->track->tfhd.flags)
	{
		file_writer_wb32(mov->fp, mov->track->tfhd.default_sample_size); /* default_sample_size*/
		size += 4;
	}

	if (MOV_TFHD_FLAG_DEFAULT_FLAGS & mov->track->tfhd.flags)
	{
		file_writer_wb32(mov->fp, mov->track->tfhd.default_sample_flags); /* default_sample_flags*/
		size += 4;
	}

	mov_write_size(mov->fp, offset, size);
	return size;
}
