#include "mov-internal.h"
#include <assert.h>

// 4.3 File Type Box (p17)
int mov_read_ftyp(struct mov_t* mov, const struct mov_box_t* box)
{
	if(box->size < 8) return -1;

	mov->ftyp.major_brand = mov_buffer_r32(&mov->io);
	mov->ftyp.minor_version = mov_buffer_r32(&mov->io);

	for(mov->ftyp.brands_count = 0; mov->ftyp.brands_count < N_BRAND && (uint64_t)mov->ftyp.brands_count * 4 < box->size - 8; ++mov->ftyp.brands_count)
	{
		mov->ftyp.compatible_brands[mov->ftyp.brands_count] = mov_buffer_r32(&mov->io);
	}

	assert(box->size == 4 * (uint64_t)mov->ftyp.brands_count + 8);
	mov_buffer_skip(&mov->io, box->size - 4 * (uint64_t)mov->ftyp.brands_count - 8 ); // skip compatible_brands
	return 0;
}

size_t mov_write_ftyp(const struct mov_t* mov)
{
	int size, i;

	size = 8/* box */ + 8/* item */ + mov->ftyp.brands_count * 4 /* compatible brands */;

	mov_buffer_w32(&mov->io, size); /* size */
	mov_buffer_write(&mov->io, "ftyp", 4);
	mov_buffer_w32(&mov->io, mov->ftyp.major_brand);
	mov_buffer_w32(&mov->io, mov->ftyp.minor_version);

	for (i = 0; i < mov->ftyp.brands_count; i++)
		mov_buffer_w32(&mov->io, mov->ftyp.compatible_brands[i]);

	return size;
}

size_t mov_write_styp(const struct mov_t* mov)
{
	int size, i;

	size = 8/* box */ + 8/* item */ + mov->ftyp.brands_count * 4 /* compatible brands */;

	mov_buffer_w32(&mov->io, size); /* size */
	mov_buffer_write(&mov->io, "styp", 4);
	mov_buffer_w32(&mov->io, mov->ftyp.major_brand);
	mov_buffer_w32(&mov->io, mov->ftyp.minor_version);

	for (i = 0; i < mov->ftyp.brands_count; i++)
		mov_buffer_w32(&mov->io, mov->ftyp.compatible_brands[i]);

	return size;
}
