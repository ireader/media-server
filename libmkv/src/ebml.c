#include "ebml.h"
#include <assert.h>

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

static uint64_t ebml_read_uint64(struct ebml_t* ebml, int max)
{
    size_t n;
    uint64_t v;

    if (ebml->err || ebml->off >= ebml->len)
    {
        ebml->err = 1;
        return 0;
    }

    n = 8 - s_ebml_log2_tab[ebml->ptr[ebml->off]];
    if (n > (size_t)max || n + ebml->off > ebml->len)
    {
        ebml->err = 1;
        return 0;
    }

    v = ebml->ptr[ebml->off] & ~(1 << s_ebml_log2_tab[ebml->ptr[ebml->off]]);
    for (++ebml->off; n-- > 1; ebml->off++)
        v = (v << 8) | ebml->ptr[ebml->off];

    return v;
}

unsigned int ebml_size_length(uint64_t size)
{
    unsigned int bytes = 0;
    size += 1;
    do {
        bytes++;
    } while (size >> (bytes * 7));
    return bytes;
}

unsigned int ebml_uint_length(uint64_t v)
{
    unsigned int bytes = 0;
    do {
        bytes++;
    } while (v >> (bytes * 8));
    return bytes;
}

unsigned int ebml_write_uint(uint8_t buf[8], uint64_t v)
{
    unsigned int i, bytes = 0;
    do {
        bytes++;
    } while (v >> (bytes * 8));

    for (i = 0; i < bytes; i++)
        buf[i] = (uint8_t)(v >> 8 * (bytes - 1 - i));

    return bytes;
}

unsigned int ebml_write_element(uint8_t buf[12], uint32_t id, uint64_t size, unsigned int bytes)
{
    unsigned int i = 0;

    assert(id > 0);
    i = ebml_write_uint(buf, id);
    assert(i <= 4 && i > 0);

    bytes = 0 == bytes ? ebml_size_length(size) : bytes;
    assert(bytes <= 8 && bytes > 0);

    size |= (1ULL << bytes * 7);
    while (bytes > 0)
    {
        buf[i++] = (uint8_t)(size >> 8 * (bytes - 1));
        bytes--;
    }

    return i;
}
