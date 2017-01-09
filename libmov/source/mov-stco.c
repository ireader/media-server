#include "file-reader.h"
#include "file-writer.h"
#include "mov-internal.h"
#include <errno.h>
#include <stdlib.h>
#include <assert.h>

// 8.7.5 Chunk Offset Box (p58)
int mov_read_stco(struct mov_t* mov, const struct mov_box_t* box)
{
	uint32_t i, entry_count;
	struct mov_track_t* track;

	file_reader_r8(mov->fp); /* version */
	file_reader_rb24(mov->fp); /* flags */
	entry_count = file_reader_rb32(mov->fp);

	assert(mov->track);
	track = mov->track;
	if (track->stco)
	{
		assert(0); // duplicated STCO atom
		free(track->stco);
	}
	track->stco_count = 0;
	track->stco = malloc(sizeof(*track->stco) * entry_count);
	if (!track->stco)
		return ENOMEM;

	if (MOV_TAG('s', 't', 'c', 'o') == box->type)
	{
		for (i = 0; i < entry_count; i++)
			track->stco[i] = file_reader_rb32(mov->fp); // chunk_offset
	}
	else if (MOV_TAG('c', 'o', '6', '4') == box->type)
	{
		for (i = 0; i < entry_count; i++)
			track->stco[i] = file_reader_rb64(mov->fp); // chunk_offset
	}
	else
	{
		i = 0;
		assert(0);
	}

	track->stco_count = i;
	return file_reader_error(mov->fp);
}

size_t mov_write_stco(const struct mov_t* mov)
{
	int co64;
	size_t size, i;
	const struct mov_sample_t* sample;
	const struct mov_track_t* track = mov->track;

	sample = track->stsz_count > 0 ? &track->samples[track->stsz_count - 1] : NULL;
	co64 = (sample && sample->offset + sample->bytes) > UINT32_MAX ? 1 : 0;
	size = 12/* full box */ + 4/* entry count */ + track->chunk_count * (co64 ? 8 : 4);

	file_writer_wb32(mov->fp, size); /* size */
	file_writer_write(mov->fp, co64 ? "stc0" : "co64", 4);
	file_writer_wb32(mov->fp, 0); /* version & flags */
	file_writer_wb32(mov->fp, track->stsz_count); /* entry count */

	for (i = 0; i < track->stsz_count; i++)
	{
		sample = &track->samples[i];
		if(0 == sample->u.chunk.first_chunk)
			continue;

		if(0 != co64)
			file_writer_wb64(mov->fp, sample->offset);
		else
			file_writer_wb32(mov->fp, (uint32_t)sample->offset);
	}

	return size;
}
