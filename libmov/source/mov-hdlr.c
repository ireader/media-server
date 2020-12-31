#include "mov-internal.h"
#include <string.h>
#include <assert.h>

// 8.4.3 Handler Reference Box (p36)
// Box Type: 'hdlr'
// Container: Media Box ('mdia') or Meta Box ('meta')
// Mandatory: Yes 
// Quantity: Exactly one
int mov_read_hdlr(struct mov_t* mov, const struct mov_box_t* box)
{
	struct mov_track_t* track = mov->track;

	mov_buffer_r8(&mov->io); /* version */
	mov_buffer_r24(&mov->io); /* flags */
	//uint32_t pre_defined = mov_buffer_r32(&mov->io);
	mov_buffer_skip(&mov->io, 4);
	track->handler_type = mov_buffer_r32(&mov->io);
	// const unsigned int(32)[3] reserved = 0;
	mov_buffer_skip(&mov->io, 12);
	// string name;
	mov_buffer_skip(&mov->io, box->size - 24); // String name
	return 0;
}

size_t mov_write_hdlr(const struct mov_t* mov)
{
	const struct mov_track_t* track = mov->track;

	mov_buffer_w32(&mov->io, 33 + (uint32_t)strlen(track->handler_descr)); /* size */
	mov_buffer_write(&mov->io, "hdlr", 4);
	mov_buffer_w32(&mov->io, 0); /* Version & flags */

	mov_buffer_w32(&mov->io, 0); /* pre_defined */
	mov_buffer_w32(&mov->io, track->handler_type); /* handler_type */

	mov_buffer_w32(&mov->io, 0); /* reserved */
	mov_buffer_w32(&mov->io, 0); /* reserved */
	mov_buffer_w32(&mov->io, 0); /* reserved */

	mov_buffer_write(&mov->io, track->handler_descr, (uint64_t)strlen(track->handler_descr)+1); /* name */
	return 33 + strlen(track->handler_descr);
}
