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

// https://developer.apple.com/library/archive/documentation/QuickTime/QTFF/QTFFChap2/qtff2.html#//apple_ref/doc/uid/TP40000939-CH204-25675
/*
Size: A 32-bit integer that specifies the number of bytes in this base media info atom.
Type: A 32-bit integer that identifies the atom type; this field must be set to 'gmin'.
Version: A 1-byte specification of the version of this base media information header atom.
Flags: A 3-byte space for base media information flags. Set this field to 0.
Graphics mode: A 16-bit integer that specifies the transfer mode. The transfer mode specifies which Boolean operation QuickDraw should perform when drawing or transferring an image from one location to another. See Graphics Modes for more information about graphics modes supported by QuickTime.
Opcolor: Three 16-bit values that specify the red, green, and blue colors for the transfer mode operation indicated in the graphics mode field.
Balance: A 16-bit integer that specifies the sound balance of this media. Sound balance is the setting that controls the mix of sound between the two speakers of a computer. This field is normally set to 0. See Balance for more information about balance values.
Reserved: Reserved for use by Apple. A 16-bit integer. Set this field to 0
*/
int mov_read_gmin(struct mov_t* mov, const struct mov_box_t* box)
{
	mov_buffer_r8(&mov->io); /* version */
	mov_buffer_r24(&mov->io); /* flags */
	mov_buffer_r16(&mov->io); /* graphics mode */
	mov_buffer_r16(&mov->io); /* opcolor red*/
	mov_buffer_r16(&mov->io); /* opcolor green*/
	mov_buffer_r16(&mov->io); /* opcolor blue*/
	mov_buffer_r16(&mov->io); /* balance */
	mov_buffer_r16(&mov->io); /* reserved */

	(void)box;
	return 0;
}

// https://developer.apple.com/library/archive/documentation/QuickTime/QTFF/QTFFChap3/qtff3.html#//apple_ref/doc/uid/TP40000939-CH205-SW90
/*
Size:A 32-bit integer that specifies the number of bytes in this text media information atom.
Type:A 32-bit integer that identifies the atom type; this field must be set to 'text'.
Matrix structure:A matrix structure associated with this text media
*/
int mov_read_text(struct mov_t* mov, const struct mov_box_t* box)
{
	int i;
	// Matrix structure
	for (i = 0; i < 9; i++)
		mov_buffer_r32(&mov->io);

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
	aligned(8) class SubtitleMediaHeaderBox extends FullBox ('sthd', version = 0, flags = 0){
}
*/
size_t mov_write_sthd(const struct mov_t* mov)
{
	mov_buffer_w32(&mov->io, 12); /* size */
	mov_buffer_write(&mov->io, "sthd", 4);
	mov_buffer_w32(&mov->io, 0); /* version & flags */
	return 12;
}
