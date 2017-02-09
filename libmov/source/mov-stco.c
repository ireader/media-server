#include "file-reader.h"
#include "file-writer.h"
#include "mov-internal.h"
#include <errno.h>
#include <stdlib.h>
#include <assert.h>

// 8.7.5 Chunk Offset Box (p58)
/*
aligned(8) class ChunkOffsetBox extends FullBox(¡®stco¡¯, version = 0, 0) { 
	unsigned int(32) entry_count; 
	for (i=1; i <= entry_count; i++) { 
		unsigned int(32) chunk_offset; 
	} 
}

aligned(8) class ChunkLargeOffsetBox extends FullBox(¡®co64¡¯, version = 0, 0) { 
	unsigned int(32) entry_count; 
	for (i=1; i <= entry_count; i++) { 
		unsigned int(64) chunk_offset; 
	} 
}
*/

int mov_read_stco(struct mov_t* mov, const struct mov_box_t* box)
{
	uint32_t i, entry_count;
	struct mov_stbl_t* stbl = &mov->track->stbl;

	file_reader_r8(mov->fp); /* version */
	file_reader_rb24(mov->fp); /* flags */
	entry_count = file_reader_rb32(mov->fp);

	assert(0 == stbl->stco_count && NULL == stbl->stco);
	if (stbl->stco_count < entry_count)
	{
		void* p = realloc(stbl->stco, sizeof(stbl->stco[0]) * entry_count);
		if (NULL == p) return ENOMEM;
		stbl->stco = p;
	}
	stbl->stco_count = entry_count;

	if (MOV_TAG('s', 't', 'c', 'o') == box->type)
	{
		for (i = 0; i < entry_count; i++)
			stbl->stco[i] = file_reader_rb32(mov->fp); // chunk_offset
	}
	else if (MOV_TAG('c', 'o', '6', '4') == box->type)
	{
		for (i = 0; i < entry_count; i++)
			stbl->stco[i] = file_reader_rb64(mov->fp); // chunk_offset
	}
	else
	{
		i = 0;
		assert(0);
	}

	stbl->stco_count = i;
	return file_reader_error(mov->fp);
}

size_t mov_write_stco(const struct mov_t* mov, uint32_t count)
{
	int co64;
	size_t size, i, j = 0;
	const struct mov_sample_t* sample;
	const struct mov_track_t* track = mov->track;

	sample = track->sample_count > 0 ? &track->samples[track->sample_count - 1] : NULL;
	co64 = (sample && sample->offset + sample->bytes > UINT32_MAX) ? 1 : 0;
	size = 12/* full box */ + 4/* entry count */ + count * (co64 ? 8 : 4);

	file_writer_wb32(mov->fp, size); /* size */
	file_writer_write(mov->fp, co64 ? "stco" : "co64", 4);
	file_writer_wb32(mov->fp, 0); /* version & flags */
	file_writer_wb32(mov->fp, count); /* entry count */

	for (i = 0; i < track->sample_count; i++)
	{
		sample = track->samples + i;
		if(0 == sample->u.stsc.first_chunk)
			continue;

		++j;
		if(0 == co64)
			file_writer_wb32(mov->fp, (uint32_t)sample->offset); 
		else
			file_writer_wb64(mov->fp, sample->offset);
	}

	assert(j == count);
	return size;
}
