#include "file-reader.h"
#include "file-writer.h"
#include "mov-internal.h"
#include <assert.h>

// 4.3 File Type Box (p17)
int mov_read_ftyp(struct mov_t* mov, const struct mov_box_t* box)
{
	if(box->size < 8) return -1;

	mov->ftyp.major_brand = file_reader_rb32(mov->fp);
	mov->ftyp.minor_version = file_reader_rb32(mov->fp);

	for(mov->ftyp.brands_count = 0; mov->ftyp.brands_count < N_BRAND && mov->ftyp.brands_count * 4 < box->size - 8; ++mov->ftyp.brands_count)
	{
		mov->ftyp.compatible_brands[mov->ftyp.brands_count] = file_reader_rb32(mov->fp);
	}

	assert(box->size == 4 * mov->ftyp.brands_count + 8);
	file_reader_seek(mov->fp, box->size - 4 * mov->ftyp.brands_count - 8 ); // skip compatible_brands
	return 0;
}

size_t mov_write_ftyp(const struct mov_t* mov)
{
	size_t size, i;

	size = 8/* box */ + 8/* item */ + mov->ftyp.brands_count * 4 /* compatible brands */;

	file_writer_wb32(mov->fp, size); /* size */
	file_writer_write(mov->fp, "ftyp", 4);
	file_writer_wb32(mov->fp, mov->ftyp.major_brand);
	file_writer_wb32(mov->fp, mov->ftyp.minor_version);

	for (i = 0; i < mov->ftyp.brands_count; i++)
		file_writer_wb32(mov->fp, mov->ftyp.compatible_brands[i]);

	return size;
}
