#include "mkv-ioutil.h"

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

uint32_t mkv_buffer_read_id(struct mkv_ioutil_t* io)
{
    size_t n;
    uint32_t v;
    uint8_t ptr[16];

    mkv_buffer_read(io, ptr, 1);
    n = 8 - s_ebml_log2_tab[ptr[0]];
    //if (n > max)
    //{
    //    io->error = 1;
    //    return 0;
    //}

    v = ptr[0];
    while (n-- > 1 && 0 == io->error)
    {
        mkv_buffer_read(io, ptr, 1);
        v = (v << 8) | ptr[0];
    }

    return io->error ? 0 : v;
}

int64_t mkv_buffer_read_size(struct mkv_ioutil_t* io)
{
    size_t i, n;
    int64_t v;
    uint8_t ptr[16];

    if (io->error)
        return 0;

    mkv_buffer_read(io, ptr, 1);
    n = 8 - s_ebml_log2_tab[ptr[0]];
    //if (n > max)
    //{
    //    io->error = 1;
    //    return 0;
    //}

    v = ptr[0] & ~(1 << s_ebml_log2_tab[ptr[0]]);
    for(i = 1; i < n && 0 == io->error; i++)
    {
        mkv_buffer_read(io, ptr, 1);
        v = (v << 8) | ptr[0];
    }

    if (v + 1 == 1LL << (7 * n))
        v = -1; // unknown length

    return io->error ? 0 : v;
}

// https://www.matroska.org/technical/basics.html 
// EBML lacing
// 1xxx xxxx 	value -(2^6-1) to 2^6-1 (ie 0 to 2^7-2 minus 2^6-1, half of the range)
// 01xx xxxx xxxx xxxx 	value -(2^13-1) to 2^13-1
// 001x xxxx xxxx xxxx xxxx xxxx 	value -(2^20-1) to 2^20-1
// 0001 xxxx xxxx xxxx xxxx xxxx xxxx xxxx 	value -(2^27-1) to 2^27-1
// 0000 1xxx xxxx xxxx xxxx xxxx xxxx xxxx xxxx xxxx 	value -(2^34-1) to 2^34-1
// 0000 01xx xxxx xxxx xxxx xxxx xxxx xxxx xxxx xxxx xxxx xxxx 	value -(2^41-1) to 2^41-1
// 0000 001x xxxx xxxx xxxx xxxx xxxx xxxx xxxx xxxx xxxx xxxx xxxx xxxx 	value -(2^48-1) to 2^48-1
int64_t mkv_buffer_read_signed_size(struct mkv_ioutil_t* io)
{
    size_t i, n;
    int64_t v;
    uint8_t ptr[16];

    mkv_buffer_read(io, ptr, 1);
    n = 8 - s_ebml_log2_tab[ptr[0]];
    //if (n > max)
    //{
    //    io->error = 1;
    //    return 0;
    //}

    v = ptr[0] & ~(1 << s_ebml_log2_tab[ptr[0]]);
    for (i = 1; i < n && 0 == io->error; i++)
    {
        mkv_buffer_read(io, ptr, 1);
        v = (v << 8) | ptr[0];
    }

    return io->error ? 0 : v - ( (1LL << (7*n - 1)) - 1);
}

void mkv_buffer_write_signed_size(struct mkv_ioutil_t* io, int64_t size)
{
    uint64_t tmp;
    tmp = 2*(size < 0 ? size^-1 : size);
    size |= (1ULL << ebml_size_length(tmp) * 7);
    do
    {
        mkv_buffer_w8(io, (uint8_t)size);
        size >>= 8;
    } while(size > 0);
}
