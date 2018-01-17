#include "mov-internal.h"

size_t mov_write_dref(const struct mov_t* mov)
{
	mov_buffer_w32(&mov->io, 28); /* size */
	mov_buffer_write(&mov->io, "dref", 4);
	mov_buffer_w32(&mov->io, 0); /* version & flags */
	mov_buffer_w32(&mov->io, 1); /* entry count */

	mov_buffer_w32(&mov->io, 12); /* size */
	//FIXME add the alis and rsrc atom
	mov_buffer_write(&mov->io, "url ", 4);
	mov_buffer_w32(&mov->io, 1); /* version & flags */

	return 28;
}

size_t mov_write_dinf(const struct mov_t* mov)
{
	size_t size;
	uint64_t offset;

	size = 8 /* Box */;
	offset = mov_buffer_tell(&mov->io);
	mov_buffer_w32(&mov->io, 0); /* size */
	mov_buffer_write(&mov->io, "dinf", 4);

	size += mov_write_dref(mov);

	mov_write_size(mov, offset, size); /* update size */
	return size;
}
