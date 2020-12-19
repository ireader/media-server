#include "webm-ioutil.h"

static const uint8_t s_ebml_log2_tab[256] = {
    0,0,1,1,2,2,2,2,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
    5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7
};

uint32_t webm_buffer_read_id(struct webm_ioutil_t* io)
{
    size_t n;
    uint32_t v;
    uint8_t ptr[16];

    if (io->error)
        return 0;

    webm_buffer_read(io, ptr, 1);
    n = 8 - s_ebml_log2_tab[ptr[0]];
    //if (n > max)
    //{
    //    io->error = 1;
    //    return 0;
    //}

    v = ptr[0];
    while (n-- > 1 && 0 == io->error)
    {
        webm_buffer_read(io, ptr, 1);
        v = (v << 8) | ptr[0];
    }

    return v;
}

uint64_t webm_buffer_read_size(struct webm_ioutil_t* io)
{
    size_t n;
    int64_t v;
    uint8_t ptr[16];

    if (io->error)
        return 0;

    webm_buffer_read(io, ptr, 1);
    n = 8 - s_ebml_log2_tab[ptr[0]];
    //if (n > max)
    //{
    //    io->error = 1;
    //    return 0;
    //}

    v = ptr[0] & ~(1 << s_ebml_log2_tab[ptr[0]]);
    while (n-- > 1 && 0 == io->error)
    {
        webm_buffer_read(io, ptr, 1);
        v = (v << 8) | ptr[0];
    }

    return v;
}
