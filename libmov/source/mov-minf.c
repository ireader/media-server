#include "file-reader.h"
#include "file-writer.h"
#include "mov-internal.h"
#include <assert.h>

int mov_read_vmhd(struct mov_t* mov, const struct mov_box_t* box)
{
	file_reader_r8(mov->fp); /* version */
	file_reader_rb24(mov->fp); /* flags */
	uint16_t graphicsmode = file_reader_rb16(mov->fp);
	// template unsigned int(16)[3] opcolor = {0, 0, 0};
	file_reader_seek(mov->fp, 6);
	return 0;
}

int mov_read_smhd(struct mov_t* mov, const struct mov_box_t* box)
{
	file_reader_r8(mov->fp); /* version */
	file_reader_rb24(mov->fp); /* flags */
	uint16_t balance = file_reader_rb16(mov->fp);
	//const unsigned int(16) reserved = 0;
	file_reader_seek(mov->fp, 2);
	return 0;
}

size_t mov_write_minf(const struct mov_t* mov)
{
	size_t size;
	uint64_t offset;
	const struct mov_track_t* track = mov->track;

	size = 8 /* Box */;
	offset = file_writer_tell(mov->fp);
	file_writer_wb32(mov->fp, 0); /* size */
	file_writer_write(mov->fp, "minf", 4);

	if (track->stream_type == AVSTREAM_VIDEO)
	{
		file_writer_wb32(mov->fp, 20); /* size (always 0x14) */
		file_writer_write(mov->fp, "vmhd", 4);
		file_writer_wb32(mov->fp, 0x01); /* version & flags */
		file_writer_wb64(mov->fp, 0); /* reserved (graphics mode = copy) */
		size += 20;
	}
	else if (track->stream_type == AVSTREAM_AUDIO)
	{
		file_writer_wb32(mov->fp, 16); /* size */
		file_writer_write(mov->fp, "smhd", 4);
		file_writer_wb32(mov->fp, 0); /* version & flags */
		file_writer_wb16(mov->fp, 0); /* reserved (balance, normally = 0) */
		file_writer_wb16(mov->fp, 0); /* reserved */
		size += 16;
	}
	else
	{
		assert(0);
	}

	mov_write_size(mov->fp, offset, size); /* update size */
	return size;
}
