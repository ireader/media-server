#include "mov-internal.h"
#include <assert.h>
#include <stdlib.h>
#include <errno.h>

// 3GPP TS 26.245 Release 14 12 V14.0.0 (2017-03)
/*
aligned(8) class StyleRecord {
	unsigned int(16) startChar;
	unsigned int(16) endChar;
	unsigned int(16) font-ID;
	unsigned int(8) face-style-flags;
	unsigned int(8) font-size;
	unsigned int(8) text-color-rgba[4];
}

class FontRecord {
	unsigned int(16) font-ID;
	unsigned int(8) font-name-length;
	unsigned int(8) font[font-name-length];
}
class FontTableBox() extends Box('ftab') {
	unsigned int(16) entry-count;
	FontRecord font-entry[entry-count];
}
class DisparityBox() extends TextSampleModifierBox ('disp') {
	signed int(16) disparity-shift-in-16th-pel;
}
class BoxRecord {
	signed int(16) top;
	signed int(16) left;
	signed int(16) bottom;
	signed int(16) right;
}
class TextSampleEntry() extends SampleEntry ('tx3g') {
	unsigned int(32) displayFlags;
	signed int(8) horizontal-justification;
	signed int(8) vertical-justification;
	unsigned int(8) background-color-rgba[4];
	BoxRecord default-text-box;
	StyleRecord default-style;
	FontTableBox font-table;
	DisparityBox default-disparity;
}
*/

int mov_read_tx3g(struct mov_t* mov, const struct mov_box_t* box)
{
	struct mov_box_t extra;
	//struct mov_track_t* track = mov->track;
	//struct mov_sample_entry_t* entry = track->stsd.current;

	mov_buffer_r32(&mov->io); // displayFlags
	mov_buffer_r8(&mov->io); // horizontal-justification
	mov_buffer_r8(&mov->io); // vertical-justification
	mov_buffer_r8(&mov->io); // background-color-rgba[4]
	mov_buffer_r8(&mov->io);
	mov_buffer_r8(&mov->io);
	mov_buffer_r8(&mov->io);
	mov_buffer_r16(&mov->io); // BoxRecord.top
	mov_buffer_r16(&mov->io); // BoxRecord.left
	mov_buffer_r16(&mov->io); // BoxRecord.bottom
	mov_buffer_r16(&mov->io); // BoxRecord.right
	mov_buffer_r16(&mov->io); // StyleRecord.startChar
	mov_buffer_r16(&mov->io); // StyleRecord.endChar
	mov_buffer_r16(&mov->io); // StyleRecord.font-ID
	mov_buffer_r8(&mov->io); // StyleRecord.face-style-flags
	mov_buffer_r8(&mov->io); // StyleRecord.font-size
	mov_buffer_r8(&mov->io); // StyleRecord.text-color-rgba[4]
	mov_buffer_r8(&mov->io);
	mov_buffer_r8(&mov->io);
	mov_buffer_r8(&mov->io);

	// FontTableBox
	extra.type = box->type;
	extra.size = box->size - 30;
	return mp4_read_extra(mov, &extra);
}

size_t mov_write_tx3g(const struct mov_t* mov)
{
	//const struct mov_track_t* track = mov->track;
	//const struct mov_sample_entry_t* entry = track->stsd.current;

	mov_buffer_w32(&mov->io, 0); // displayFlags
	mov_buffer_w8(&mov->io, 0x01); // horizontal-justification
	mov_buffer_w8(&mov->io, 0xFF); // vertical-justification
	mov_buffer_w8(&mov->io, 0x00); // background-color-rgba[4]
	mov_buffer_w8(&mov->io, 0x00);
	mov_buffer_w8(&mov->io, 0x00);
	mov_buffer_w8(&mov->io, 0x00);
	mov_buffer_w16(&mov->io, 0x0000); // BoxRecord.top
	mov_buffer_w16(&mov->io, 0x0000); // BoxRecord.left
	mov_buffer_w16(&mov->io, 0x0000); // BoxRecord.bottom
	mov_buffer_w16(&mov->io, 0x0000); // BoxRecord.right
	mov_buffer_w16(&mov->io, 0x0000); // StyleRecord.startChar
	mov_buffer_w16(&mov->io, 0x0000); // StyleRecord.endChar
	mov_buffer_w16(&mov->io, 0x0001); // StyleRecord.font-ID
	mov_buffer_w8(&mov->io, 0x00); // StyleRecord.face-style-flags
	mov_buffer_w8(&mov->io, 0x12); // StyleRecord.font-size
	mov_buffer_w8(&mov->io, 0xFF); // StyleRecord.text-color-rgba[4]
	mov_buffer_w8(&mov->io, 0xFF);
	mov_buffer_w8(&mov->io, 0xFF);
	mov_buffer_w8(&mov->io, 0xFF);

	// FontTableBox
	mov_buffer_w32(&mov->io, 18); /* size */
	mov_buffer_write(&mov->io, "ftab", 4);
	mov_buffer_w16(&mov->io, 1); /* entry-count */
	mov_buffer_w16(&mov->io, 0x0001); /* FontRecord.font-ID */
	mov_buffer_w8(&mov->io, 5); /* FontRecord.font-name-length */
	mov_buffer_write(&mov->io, "Serif", 5); /* FontRecord.font[font-name-length] */

	return 30 + 18;
}
