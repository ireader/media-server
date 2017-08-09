#include "file-reader.h"
#include "file-writer.h"
#include "mov-internal.h"
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

#define MOV_TREX_FLAG_IS_LEADING_MASK					0x0C000000
#define MOV_TREX_FLAG_SAMPLE_DEPENDS_ON_MASK			0x03000000
#define MOV_TREX_FLAG_SAMPLE_IS_DEPENDED_ON_MASK		0x00C00000
#define MOV_TREX_FLAG_SAMPLE_HAS_REDUNDANCY_MASK		0x00300000
#define MOV_TREX_FLAG_SAMPLE_PADDING_VALUE_MASK			0x000E0000
#define MOV_TREX_FLAG_SAMPLE_IS_NO_SYNC_SAMPLE			0x00010000
#define MOV_TREX_FLAG_SAMPLE_DEGRADATION_PRIORITY_MASK	0x0000FFFF

#define MOV_TRUN_FLAG_DATA_OFFSET_PRESENT						0x0001
#define MOV_TRUN_FLAG_FIRST_SAMPLE_FLAGS_PRESENT				0x0004
#define MOV_TRUN_FLAG_SAMPLE_DURATION_PRESENT					0x0100
#define MOV_TRUN_FLAG_SAMPLE_SIZE_PRESENT						0x0200
#define MOV_TRUN_FLAG_SAMPLE_FLAGS_PRESENT						0x0400
#define MOV_TRUN_FLAG_SAMPLE_COMPOSITION_TIME_OFFSET_PRESENT	0x0800

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
		sample[i].flags = (sample_flags & (MOV_TREX_FLAG_SAMPLE_IS_NO_SYNC_SAMPLE | 0x01000000)) ? 0 : MOV_AV_FLAG_KEYFREAME;
		sample[i].stsd = mov_track_dref_find(track, track->tfhd.sample_description_index);
		sample[i].dts = track->trex.dts;
		sample[i].pts = sample[i].dts + sample_composition_time_offset;

		data_offset += sample_size;
		track->trex.dts += sample_duration;
	}
	track->sample_count += sample_count;

	return file_reader_error(mov->fp); (void)box;
}
