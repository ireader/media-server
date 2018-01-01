#include "file-reader.h"
#include "file-writer.h"
#include "mov-internal.h"
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

// 8.8.8 Track Fragment Run Box (p72)
int mov_read_trun(struct mov_t* mov, const struct mov_box_t* box)
{
	unsigned int version;
	uint32_t flags;
	uint32_t i, sample_count;
	uint64_t data_offset;
	uint32_t first_sample_flags;
	uint32_t sample_duration, sample_size, sample_flags;
	int64_t sample_composition_time_offset;
	struct mov_track_t* track;
	struct mov_sample_t* sample;

	version = file_reader_r8(mov->fp); /* version */
	flags = file_reader_rb24(mov->fp); /* flags */
	sample_count = file_reader_rb32(mov->fp); /* sample_count */

	track = mov->track;
	if (track->sample_count + sample_count + 1 > track->sample_offset)
	{
		void* p = realloc(track->samples, sizeof(struct mov_sample_t) * (track->sample_count + 2*sample_count + 1));
		if (NULL == p) return ENOMEM;
		track->samples = (struct mov_sample_t*)p;
		track->sample_offset = track->sample_count + 2 * sample_count + 1;
	}

	data_offset = track->tfhd.base_data_offset;
	if (MOV_TRUN_FLAG_DATA_OFFSET_PRESENT & flags)
		data_offset += (int32_t)file_reader_rb32(mov->fp); /* data_offset */

	if (MOV_TRUN_FLAG_FIRST_SAMPLE_FLAGS_PRESENT & flags)
		first_sample_flags = (int32_t)file_reader_rb32(mov->fp); /* first_sample_flags */
	else
		first_sample_flags = track->tfhd.flags;

	sample = track->samples + track->sample_count;
	for (i = 0; i < sample_count; i++)
	{
		if (MOV_TRUN_FLAG_SAMPLE_DURATION_PRESENT & flags)
			sample_duration = file_reader_rb32(mov->fp); /* sample_duration*/
		else
			sample_duration = track->tfhd.default_sample_duration;

		if (MOV_TRUN_FLAG_SAMPLE_SIZE_PRESENT & flags)
			sample_size = file_reader_rb32(mov->fp); /* sample_size*/
		else
			sample_size = track->tfhd.default_sample_size;

		if (MOV_TRUN_FLAG_SAMPLE_FLAGS_PRESENT & flags)
			sample_flags = file_reader_rb32(mov->fp); /* sample_flags*/
		else
			sample_flags = i ? track->tfhd.default_sample_flags : first_sample_flags;

		if (MOV_TRUN_FLAG_SAMPLE_COMPOSITION_TIME_OFFSET_PRESENT & flags)
		{
			sample_composition_time_offset = file_reader_rb32(mov->fp); /* sample_composition_time_offset*/
			if (1 == version)
				sample_composition_time_offset = (int32_t)sample_composition_time_offset;
		}
		else
			sample_composition_time_offset = 0;

		sample[i].offset = data_offset;
		sample[i].bytes = sample_size;
		sample[i].dts = track->end_dts;
		sample[i].pts = sample[i].dts + sample_composition_time_offset;
		sample[i].flags = (sample_flags & (MOV_TREX_FLAG_SAMPLE_IS_NO_SYNC_SAMPLE | 0x01000000)) ? 0 : MOV_AV_FLAG_KEYFREAME;
		sample[i].sample_description_index = track->tfhd.sample_description_index;

		data_offset += sample_size;
		track->end_dts += sample_duration;
	}
	track->sample_count += sample_count;

	return file_reader_error(mov->fp); (void)box;
}

size_t mov_write_trun(const struct mov_t* mov, uint32_t flags, uint32_t flags0, size_t from, size_t count)
{
	uint32_t delta;
	uint64_t offset;
	size_t size, i;
	const struct mov_sample_t* sample;
	const struct mov_track_t* track = mov->track;

	size = 12/* full box */ + 4/* sample count */;

	offset = file_writer_tell(mov->fp);
	file_writer_wb32(mov->fp, 0); /* size */
	file_writer_write(mov->fp, "trun", 4);
	file_writer_w8(mov->fp, 1); /* version */
	file_writer_wb24(mov->fp, flags); /* flags */
	file_writer_wb32(mov->fp, count); /* sample_count */

	if (flags & MOV_TRUN_FLAG_DATA_OFFSET_PRESENT)
	{
		file_writer_wb32(mov->fp, 0); /* data_offset, rewrite on fmp4_write_fragment */
		size += 4;;
	}

	if (flags & MOV_TRUN_FLAG_FIRST_SAMPLE_FLAGS_PRESENT)
	{
		file_writer_wb32(mov->fp, flags0); /* first_sample_flags */
		size += 4;;
	}

	assert(from + count <= track->sample_count);
	for (i = from; i < from + count; i++)
	{
		sample = track->samples + i;
		if (flags & MOV_TRUN_FLAG_SAMPLE_DURATION_PRESENT)
		{
			delta = (uint32_t)(i + 1 < track->sample_count ? track->samples[i + 1].dts - track->samples[i].dts : 0);
			file_writer_wb32(mov->fp, delta); /* sample_duration */
			size += 4;
		}

		if (flags & MOV_TRUN_FLAG_SAMPLE_SIZE_PRESENT)
		{
			file_writer_wb32(mov->fp, (uint32_t)sample->bytes); /* sample_size */
			size += 4;
		}

		assert(0 == (flags & MOV_TRUN_FLAG_SAMPLE_FLAGS_PRESENT));
//		file_writer_wb32(mov->fp, 0); /* sample_flags */

		if (flags & MOV_TRUN_FLAG_SAMPLE_COMPOSITION_TIME_OFFSET_PRESENT)
		{
			file_writer_wb32(mov->fp, (int32_t)(sample->pts - sample->dts)); /* sample_composition_time_offset */
			size += 4;
		}
	}

	mov_write_size(mov->fp, offset, size);
	return size;
}
