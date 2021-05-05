#include "mov-internal.h"
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

// 8.8.10 Track Fragment Random Access Box (p74)
int mov_read_tfra(struct mov_t* mov, const struct mov_box_t* box)
{
	unsigned int version;
	uint32_t track_ID;
	uint32_t length_size_of;
	uint32_t i, j, number_of_entry;
	uint32_t traf_number, trun_number, sample_number;
	struct mov_track_t* track;

	version = mov_buffer_r8(&mov->io); /* version */
	mov_buffer_r24(&mov->io); /* flags */
	track_ID = mov_buffer_r32(&mov->io); /* track_ID */

	track = mov_find_track(mov, track_ID);
	if (NULL == track)
		return -1;

	length_size_of = mov_buffer_r32(&mov->io); /* length_size_of XXX */
	number_of_entry = mov_buffer_r32(&mov->io); /* number_of_entry */
	if (number_of_entry > 0)
	{
		void* p = realloc(track->frags, sizeof(struct mov_fragment_t) * number_of_entry);
		if (!p) return ENOMEM;
		track->frags = p;
	}	
	track->frag_count = number_of_entry;

	for (i = 0; i < number_of_entry; i++)
	{
		if (1 == version)
		{
			track->frags[i].time = mov_buffer_r64(&mov->io); /* time */
			track->frags[i].offset = mov_buffer_r64(&mov->io); /* moof_offset */
		}
		else
		{
			track->frags[i].time = mov_buffer_r32(&mov->io); /* time */
			track->frags[i].offset = mov_buffer_r32(&mov->io); /* moof_offset */
		}

		for (traf_number = 0, j = 0; j < ((length_size_of >> 4) & 0x03) + 1; j++)
			traf_number = (traf_number << 8) | mov_buffer_r8(&mov->io); /* traf_number */

		for (trun_number = 0, j = 0; j < ((length_size_of >> 2) & 0x03) + 1; j++)
			trun_number = (trun_number << 8) | mov_buffer_r8(&mov->io); /* trun_number */

		for (sample_number = 0, j = 0; j < (length_size_of & 0x03) + 1; j++)
			sample_number = (sample_number << 8) | mov_buffer_r8(&mov->io); /* sample_number */
	}

	(void)box;
	return mov_buffer_error(&mov->io);
}

size_t mov_write_tfra(const struct mov_t* mov)
{
	uint32_t i, size;
	const struct mov_track_t* track = mov->track;

	size = 12/* full box */ + 12/* base */ + track->frag_count * 19/* index */;

	mov_buffer_w32(&mov->io, size); /* size */
	mov_buffer_write(&mov->io, "tfra", 4);
	mov_buffer_w8(&mov->io, 1); /* version */
	mov_buffer_w24(&mov->io, 0); /* flags */

	mov_buffer_w32(&mov->io, track->tkhd.track_ID); /* track_ID */
	mov_buffer_w32(&mov->io, 0); /* length_size_of_traf_num/trun/sample */
	mov_buffer_w32(&mov->io, track->frag_count); /* number_of_entry */

	for (i = 0; i < track->frag_count; i++)
	{
		mov_buffer_w64(&mov->io, track->frags[i].time);
		mov_buffer_w64(&mov->io, track->frags[i].offset); /* moof_offset */
		mov_buffer_w8(&mov->io, 1); /* traf number */
		mov_buffer_w8(&mov->io, 1); /* trun number */
		mov_buffer_w8(&mov->io, 1); /* sample number */
	}

	return size;
}
