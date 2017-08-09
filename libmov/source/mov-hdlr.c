#include "file-reader.h"
#include "file-writer.h"
#include "mov-internal.h"
#include <string.h>
#include <assert.h>

// 8.4.3 Handler Reference Box (p36)
// Box Type: ¡®hdlr¡¯ 
// Container: Media Box (¡®mdia¡¯) or Meta Box (¡®meta¡¯) 
// Mandatory: Yes 
// Quantity: Exactly one
int mov_read_hdlr(struct mov_t* mov, const struct mov_box_t* box)
{
	struct mov_track_t* track = mov->track;

	file_reader_r8(mov->fp); /* version */
	file_reader_rb24(mov->fp); /* flags */
	//uint32_t pre_defined = file_reader_rb32(mov->fp);
	file_reader_skip(mov->fp, 4);
	track->handler_type = file_reader_rb32(mov->fp);
	// const unsigned int(32)[3] reserved = 0;
	file_reader_skip(mov->fp, 12);
	// string name;
	file_reader_skip(mov->fp, box->size - 24); // String name
	return 0;
}

size_t mov_write_hdlr(const struct mov_t* mov)
{
	const struct mov_track_t* track = mov->track;

	file_writer_wb32(mov->fp, 33 + strlen(track->handler_descr)); /* size */
	file_writer_write(mov->fp, "hdlr", 4);
	file_writer_wb32(mov->fp, 0); /* Version & flags */

	file_writer_wb32(mov->fp, 0); /* pre_defined */
	file_writer_wb32(mov->fp, track->handler_type); /* handler_type */

	file_writer_wb32(mov->fp, 0); /* reserved */
	file_writer_wb32(mov->fp, 0); /* reserved */
	file_writer_wb32(mov->fp, 0); /* reserved */

	file_writer_write(mov->fp, track->handler_descr, strlen(track->handler_descr)+1); /* name */
	return 33 + strlen(track->handler_descr);
}
