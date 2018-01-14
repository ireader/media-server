#include "mov-internal.h"
#include <assert.h>
#include <stdlib.h>
#include <errno.h>

// extra_data: ISO/IEC 14496-15:2017 HEVCDecoderConfigurationRecord

int mov_read_hvcc(struct mov_t* mov, const struct mov_box_t* box)
{
	struct mov_track_t* track = mov->track;
	if (track->extra_data_size < box->size)
	{
		void* p = realloc(track->extra_data, (size_t)box->size);
		if (NULL == p) return ENOMEM;
		track->extra_data = p;
	}

	mov_buffer_read(&mov->io, track->extra_data, (size_t)box->size);
	track->extra_data_size = (size_t)box->size;
	return mov_buffer_error(&mov->io);
}

size_t mov_write_hvcc(const struct mov_t* mov)
{
	const struct mov_track_t* track = mov->track;
	mov_buffer_w32(&mov->io, track->extra_data_size + 8); /* size */
	mov_buffer_write(&mov->io, "hvcC", 4);
	if (track->extra_data_size > 0)
		mov_buffer_write(&mov->io, track->extra_data, track->extra_data_size);
	return track->extra_data_size + 8;
}
