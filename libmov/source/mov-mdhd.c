#include "mov-internal.h"
#include <assert.h>

// 8.4.2 Media Header Box (p35)
// Box Type: ¡®mdhd¡¯ 
// Container: Media Box (¡®mdia¡¯) 
// Mandatory: Yes 
// Quantity: Exactly one

/*
aligned(8) class MediaHeaderBox extends FullBox(¡®mdhd¡¯, version, 0) { 
	if (version==1) { 
		unsigned int(64) creation_time; 
		unsigned int(64) modification_time; 
		unsigned int(32) timescale; 
		unsigned int(64) duration; 
	} else { // version==0 
		unsigned int(32) creation_time; 
		unsigned int(32) modification_time; 
		unsigned int(32) timescale; 
		unsigned int(32) duration; 
	} 
	bit(1) pad = 0; 
	unsigned int(5)[3] language; // ISO-639-2/T language code 
	unsigned int(16) pre_defined = 0; 
}
*/
int mov_read_mdhd(struct mov_t* mov, const struct mov_box_t* box)
{
	uint32_t val;
	struct mov_mdhd_t* mdhd = &mov->track->mdhd;

	mdhd->version = mov_buffer_r8(&mov->io);
	mdhd->flags = mov_buffer_r24(&mov->io);

	if (1 == mdhd->version)
	{
		mdhd->creation_time = mov_buffer_r64(&mov->io);
		mdhd->modification_time = mov_buffer_r64(&mov->io);
		mdhd->timescale = mov_buffer_r32(&mov->io);
		mdhd->duration = mov_buffer_r64(&mov->io);
	}
	else
	{
		assert(0 == mdhd->version);
		mdhd->creation_time = mov_buffer_r32(&mov->io);
		mdhd->modification_time = mov_buffer_r32(&mov->io);
		mdhd->timescale = mov_buffer_r32(&mov->io);
		mdhd->duration = mov_buffer_r32(&mov->io);
	}

	val = mov_buffer_r32(&mov->io);
	mdhd->language = (val >> 16) & 0x7FFF;
	mdhd->pre_defined = val & 0xFFFF;

	(void)box;
	return mov_buffer_error(&mov->io);
}

size_t mov_write_mdhd(const struct mov_t* mov)
{
	const struct mov_mdhd_t* mdhd = &mov->track->mdhd;
	
	mov_buffer_w32(&mov->io, 32); /* size */
	mov_buffer_write(&mov->io, "mdhd", 4);
	mov_buffer_w32(&mov->io, 0); /* version 1 & flags */

	mov_buffer_w32(&mov->io, (uint32_t)mdhd->creation_time); /* creation_time */
	mov_buffer_w32(&mov->io, (uint32_t)mdhd->modification_time); /* modification_time */
	mov_buffer_w32(&mov->io, mdhd->timescale); /* timescale */
	mov_buffer_w32(&mov->io, (uint32_t)mdhd->duration); /* duration */
	
	mov_buffer_w16(&mov->io, (uint16_t)mdhd->language); /* ISO-639-2/T language code */
	mov_buffer_w16(&mov->io, 0); /* pre_defined (quality) */
	return 32;
}
