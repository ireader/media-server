#include "mov-internal.h"
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

// 8.8.12 Track fragment decode time (p76)
int mov_read_tfdt(struct mov_t* mov, const struct mov_box_t* box)
{
    unsigned int version;
    version = mov_buffer_r8(&mov->io); /* version */
    mov_buffer_r24(&mov->io); /* flags */

    if (1 == version)
        mov->track->tfdt_dts = mov_buffer_r64(&mov->io); /* baseMediaDecodeTime */
    else
        mov->track->tfdt_dts = mov_buffer_r32(&mov->io); /* baseMediaDecodeTime */

    // baseMediaDecodeTime + ELST start offset
    mov_apply_elst_tfdt(mov->track);

	(void)box;
    return mov_buffer_error(&mov->io);
}

size_t mov_write_tfdt(const struct mov_t* mov)
{
    uint8_t version;
    uint64_t baseMediaDecodeTime;

    if (mov->track->sample_count < 1)
        return 0;

    baseMediaDecodeTime = mov->track->samples[0].dts - mov->track->start_dts;
    version = baseMediaDecodeTime > INT32_MAX ? 1 : 0;

    mov_buffer_w32(&mov->io, 0 == version ? 16 : 20); /* size */
    mov_buffer_write(&mov->io, "tfdt", 4);
    mov_buffer_w8(&mov->io, version); /* version */
    mov_buffer_w24(&mov->io, 0); /* flags */

    if (1 == version)
        mov_buffer_w64(&mov->io, baseMediaDecodeTime); /* baseMediaDecodeTime */
    else
        mov_buffer_w32(&mov->io, (uint32_t)baseMediaDecodeTime); /* baseMediaDecodeTime */

    return 0 == version ? 16 : 20;
}
