#include "mov-internal.h"
#include <assert.h>
#include <stdlib.h>
#include <errno.h>

// https://aomediacodec.github.io/av1-isobmff
// extra data: AV1CodecConfigurationRecord

int mov_read_av1c(struct mov_t* mov, const struct mov_box_t* box)
{
	struct mov_track_t* track = mov->track;
	struct mov_sample_entry_t* entry = track->stsd.current;
	if (entry->extra_data_size < box->size)
	{
		void* p = realloc(entry->extra_data, (size_t)box->size);
		if (NULL == p) return ENOMEM;
		entry->extra_data = p;
	}

	mov_buffer_read(&mov->io, entry->extra_data, box->size);
	entry->extra_data_size = (int)box->size;
	return mov_buffer_error(&mov->io);
}

size_t mov_write_av1c(const struct mov_t* mov)
{
	const struct mov_track_t* track = mov->track;
	const struct mov_sample_entry_t* entry = track->stsd.current;
	mov_buffer_w32(&mov->io, entry->extra_data_size + 8); /* size */
	mov_buffer_write(&mov->io, "av1C", 4);
	if (entry->extra_data_size > 0)
		mov_buffer_write(&mov->io, entry->extra_data, entry->extra_data_size);
	return entry->extra_data_size + 8;
}
