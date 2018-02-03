#include "mov-internal.h"
#include <assert.h>

int mov_read_vmhd(struct mov_t* mov, const struct mov_box_t* box)
{
	mov_buffer_r8(&mov->io); /* version */
	mov_buffer_r24(&mov->io); /* flags */
	mov_buffer_r16(&mov->io); /* graphicsmode */
	// template unsigned int(16)[3] opcolor = {0, 0, 0};
	mov_buffer_skip(&mov->io, 6);

	(void)box;
	return 0;
}

int mov_read_smhd(struct mov_t* mov, const struct mov_box_t* box)
{
	mov_buffer_r8(&mov->io); /* version */
	mov_buffer_r24(&mov->io); /* flags */
	mov_buffer_r16(&mov->io); /* balance */
	//const unsigned int(16) reserved = 0;
	mov_buffer_skip(&mov->io, 2);

	(void)box;
	return 0;
}

size_t mov_write_vmhd(const struct mov_t* mov)
{
	mov_buffer_w32(&mov->io, 20); /* size (always 0x14) */
	mov_buffer_write(&mov->io, "vmhd", 4);
	mov_buffer_w32(&mov->io, 0x01); /* version & flags */
	mov_buffer_w64(&mov->io, 0); /* reserved (graphics mode = copy) */
	return 20;
}

size_t mov_write_smhd(const struct mov_t* mov)
{
	mov_buffer_w32(&mov->io, 16); /* size */
	mov_buffer_write(&mov->io, "smhd", 4);
	mov_buffer_w32(&mov->io, 0); /* version & flags */
	mov_buffer_w16(&mov->io, 0); /* reserved (balance, normally = 0) */
	mov_buffer_w16(&mov->io, 0); /* reserved */
	return 16;
}

size_t mov_write_nmhd(const struct mov_t* mov)
{
	mov_buffer_w32(&mov->io, 12); /* size */
	mov_buffer_write(&mov->io, "nmhd", 4);
	mov_buffer_w32(&mov->io, 0); /* version & flags */
	return 12;
}

/*
ISO/IEC 14496-12:2015(E) 12.6.2 Subtitle media header (p185)
aligned(8) class SubtitleMediaHeaderBox extends FullBox (¡®sthd¡¯, version = 0, flags = 0){
}
*/
size_t mov_write_sthd(const struct mov_t* mov)
{
	mov_buffer_w32(&mov->io, 12); /* size */
	mov_buffer_write(&mov->io, "sthd", 4);
	mov_buffer_w32(&mov->io, 0); /* version & flags */
	return 12;
}
