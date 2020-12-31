#include "mov-internal.h"
#include <assert.h>
#include <stdlib.h>
#include <errno.h>

// https://www.webmproject.org/vp9/mp4/
// extra data: VPCodecConfigurationBox

int mov_read_vpcc(struct mov_t* mov, const struct mov_box_t* box)
{
    struct mov_track_t* track = mov->track;
    struct mov_sample_entry_t* entry = track->stsd.current;
    if(box->size < 4)
        return -1;
    if (entry->extra_data_size < box->size-4)
    {
        void* p = realloc(entry->extra_data, (size_t)box->size-4);
        if (NULL == p) return ENOMEM;
        entry->extra_data = p;
    }

    mov_buffer_r8(&mov->io); /* version */
    mov_buffer_r24(&mov->io); /* flags */
    mov_buffer_read(&mov->io, entry->extra_data, box->size-4);
    entry->extra_data_size = (int)box->size - 4;
    return mov_buffer_error(&mov->io);
}

size_t mov_write_vpcc(const struct mov_t* mov)
{
    const struct mov_track_t* track = mov->track;
    const struct mov_sample_entry_t* entry = track->stsd.current;
    mov_buffer_w32(&mov->io, entry->extra_data_size + 12); /* size */
    mov_buffer_write(&mov->io, "vpcC", 4);
    mov_buffer_w8(&mov->io, 1); /* version */
    mov_buffer_w24(&mov->io, 0); /* flags */
    if (entry->extra_data_size > 0)
        mov_buffer_write(&mov->io, entry->extra_data, entry->extra_data_size);
    return entry->extra_data_size + 12;
}
