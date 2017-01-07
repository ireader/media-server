#include "file-reader.h"
#include "file-writer.h"
#include "mov-internal.h"
#include <assert.h>

size_t mov_write_hdlr(const struct mov_t* mov)
{
	const struct mov_track_t* track = mov->track;

	file_writer_wb32(mov->fp, 33); /* size */
	file_writer_write(mov->fp, "hdlr", 4);
	file_writer_wb32(mov->fp, 0); /* Version & flags */

	file_writer_wb32(mov->fp, 0); /* pre_defined */
	if (track->stream_type == AVSTREAM_VIDEO)
	{
		file_writer_write(mov->fp, "vide", 4); /* handler_type */
	}
	else if (track->stream_type == AVSTREAM_AUDIO)
	{
		file_writer_write(mov->fp, "soun", 4); /* handler_type */
	}
	else
	{
		assert(0);
	}

	file_writer_wb32(mov->fp, 0); /* reserved */
	file_writer_wb32(mov->fp, 0); /* reserved */
	file_writer_wb32(mov->fp, 0); /* reserved */

	file_writer_w8(mov->fp, 0); /* name */
	return 33;
}

