#include "file-reader.h"
#include "file-writer.h"
#include "mov-internal.h"
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

// 8.6.2 Sync Sample Box (p50)
int mov_read_stss(struct mov_t* mov, const struct mov_box_t* box)
{
	uint32_t i, entry_count;
	struct mov_track_t* stream;
	file_reader_r8(mov->fp); /* version */
	file_reader_rb24(mov->fp); /* flags */
	entry_count = file_reader_rb32(mov->fp);

	assert(mov->track);
	stream = mov->track;
	if (stream->stss)
	{
		assert(0);
		free(stream->stss); // duplicated STSS atom
	}
	stream->stss_count = 0;
	stream->stss = malloc(sizeof(stream->stss[0]) * entry_count);
	if (NULL == stream->stss)
		return ENOMEM;

	for (i = 0; i < entry_count; i++)
		stream->stss[i] = file_reader_rb32(mov->fp); // uint32_t sample_number

	stream->stss_count = i;
	return 0;
}
