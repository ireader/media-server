#include "file-reader.h"
#include "mov-internal.h"
#include <errno.h>
#include <stdlib.h>
#include <assert.h>

// 8.7.4 Sample To Chunk Box (p57)
int mov_read_stsc(struct mov_reader_t* mov, const struct mov_box_t* box)
{
	uint32_t i, entry_count;
	struct mov_track_t* track;

	file_reader_r8(mov->fp); /* version */
	file_reader_rb24(mov->fp); /* flags */
	entry_count = file_reader_rb32(mov->fp);

	assert(mov->track);
	track = mov->track;
	if (track->stsc)
	{
		assert(0); // duplicated STSC atom
		free(track->stsc);
	}
	track->stsc_count = 0;
	track->stsc = malloc(sizeof(struct mov_stsc_t) * (entry_count + 1/*stco count*/));
	if (!track->stsc)
		return ENOMEM;

	for (i = 0; i < entry_count; i++)
	{
		track->stsc[i].first_chunk = file_reader_rb32(mov->fp);
		track->stsc[i].samples_per_chunk = file_reader_rb32(mov->fp);
		track->stsc[i].sample_description_index = file_reader_rb32(mov->fp);
	}

	track->stsc_count = i;
	return file_reader_error(mov->fp);
}
