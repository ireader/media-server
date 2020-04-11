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

size_t mov_write_tx3g(const struct mov_t* mov)
{
	const struct mov_track_t* track = mov->track;
	const struct mov_sample_entry_t* entry = track->stsd.current;
	mov_buffer_w32(&mov->io, entry->extra_data_size + 8); /* size */
	mov_buffer_write(&mov->io, "tx3g", 4);
	if (entry->extra_data_size > 0)
		mov_buffer_write(&mov->io, entry->extra_data, entry->extra_data_size);
	return entry->extra_data_size + 8;
}
