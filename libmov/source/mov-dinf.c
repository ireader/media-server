#include "mov-internal.h"
#include "file-reader.h"
#include "file-writer.h"

size_t mov_write_dref(const struct mov_t* mov)
{
	file_writer_wb32(mov->fp, 28); /* size */
	file_writer_write(mov->fp, "dref", 4);
	file_writer_wb32(mov->fp, 0); /* version & flags */
	file_writer_wb32(mov->fp, 1); /* entry count */

	file_writer_wb32(mov->fp, 12); /* size */
								   //FIXME add the alis and rsrc atom
	file_writer_write(mov->fp, "url ", 4);
	file_writer_wb32(mov->fp, 1); /* version & flags */

	return 28;
}

size_t mov_write_dinf(const struct mov_t* mov)
{
	size_t size;
	uint64_t offset;

	size = 8 /* Box */;
	offset = file_writer_tell(mov->fp);
	file_writer_wb32(mov->fp, 0); /* size */
	file_writer_write(mov->fp, "dinf", 4);

	size += mov_write_dref(mov);

	mov_write_size(mov->fp, offset, size); /* update size */
	return size;
}
