#include "mov-internal.h"
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

// 8.8.2 Movie Extends Header Box (p68)
int mov_read_mehd(struct mov_t* mov, const struct mov_box_t* box)
{
    unsigned int version;
    uint64_t fragment_duration;
    version = mov_buffer_r8(&mov->io); /* version */
    mov_buffer_r24(&mov->io); /* flags */

    if (1 == version)
        fragment_duration = mov_buffer_r64(&mov->io); /* fragment_duration*/
    else
        fragment_duration = mov_buffer_r32(&mov->io); /* fragment_duration*/

    (void)box;
    //assert(fragment_duration <= mov->mvhd.duration);
    return mov_buffer_error(&mov->io);
}

size_t mov_write_mehd(const struct mov_t* mov)
{
    mov_buffer_w32(&mov->io, 20); /* size */
    mov_buffer_write(&mov->io, "mehd", 4);
    mov_buffer_w8(&mov->io, 1); /* version */
    mov_buffer_w24(&mov->io, 0); /* flags */
    mov_buffer_w64(&mov->io, mov->mvhd.duration); // 0 ?
    return 20;
}
