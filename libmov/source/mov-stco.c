#include "file-reader.h"
#include "mov-internal.h"
#include <errno.h>
#include <stdlib.h>
#include <assert.h>

// 8.7.5 Chunk Offset Box (p58)
int mov_read_stco(struct mov_reader_t* mov, const struct mov_box_t* box)
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
