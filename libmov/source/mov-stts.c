#include "file-reader.h"
#include "mov-internal.h"
#include <errno.h>
#include <stdlib.h>
#include <assert.h>

// 8.6.1.2 Decoding Time to Sample Box (p47)
int mov_read_stts(struct mov_reader_t* mov, const struct mov_box_t* box)
{
	uint32_t i, entry_count;
	struct mov_track_t* track;
	file_reader_r8(mov->fp); /* version */
	file_reader_rb24(mov->fp); /* flags */
	entry_count = file_reader_rb32(mov->fp);

	assert(mov->track);
	track = mov->track;
	if (track->stts)
	{
		assert(0);
		free(track->stts); // duplicated STTS atom
	}
	track->stts_count = 0;
	track->stts = malloc(sizeof(struct mov_stts_t) * entry_count);
	if (NULL == track->stts)
		return ENOMEM;

	for (i = 0; i < entry_count; i++)
	{
		track->stts[i].sample_count = file_reader_rb32(mov->fp);
		track->stts[i].sample_delta = file_reader_rb32(mov->fp);
	}

	track->stts_count = i;
	return file_reader_error(mov->fp);
}

// 8.6.1.3 Composition Time to Sample Box (p47)
int mov_read_ctts(struct mov_reader_t* mov, const struct mov_box_t* box)
{
	uint32_t i, entry_count;
	struct mov_track_t* track;
	file_reader_r8(mov->fp); /* version */
	file_reader_rb24(mov->fp); /* flags */
	entry_count = file_reader_rb32(mov->fp);

	assert(mov->track);
	track = mov->track;
	if (track->ctts)
	{
		assert(0);
		free(track->ctts); // duplicated CTTS atom
	}
	track->ctts_count = 0;
	track->ctts = malloc(sizeof(struct mov_stts_t) * entry_count);
	if (NULL == track->ctts)
		return ENOMEM;

	for (i = 0; i < entry_count; i++)
	{
		track->ctts[i].sample_count = file_reader_rb32(mov->fp);
		track->ctts[i].sample_delta = file_reader_rb32(mov->fp); // uint32_t sample_offset
	}

	track->ctts_count = i;
	return file_reader_error(mov->fp);
}
