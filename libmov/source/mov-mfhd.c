#include "mov-internal.h"
#include <errno.h>
#include <stdlib.h>
#include <assert.h>

// 8.8.5 Movie Fragment Header Box (p70)
int mov_read_mfhd(struct mov_t* mov, const struct mov_box_t* box)
{
    (void)box;
    mov_buffer_r32(&mov->io); /* version & flags */
    mov_buffer_r32(&mov->io); /* sequence_number */
    return mov_buffer_error(&mov->io);
}

size_t mov_write_mfhd(const struct mov_t* mov, uint32_t fragment)
{
    mov_buffer_w32(&mov->io, 16); /* size */
    mov_buffer_write(&mov->io, "mfhd", 4);
    mov_buffer_w32(&mov->io, 0); /* version & flags */
    mov_buffer_w32(&mov->io, fragment); /* sequence_number */
    return 16;
}
