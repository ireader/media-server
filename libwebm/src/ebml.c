#include "ebml.h"

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

static int64_t ebml_read_int64(struct ebml_t* ebml, int max)
{
    size_t n;
    int64_t v;

    if (ebml->err || ebml->off >= ebml->len)
    {
        ebml->err = 1;
        return 0;
    }

    n = 8 - s_ebml_log2_tab[ebml->ptr[ebml->off]];
    if (n > max || n + ebml->off > ebml->len)
    {
        ebml->err = 1;
        return 0;
    }

    v = ebml->ptr[ebml->off] & ~(1 << s_ebml_log2_tab[ebml->ptr[ebml->off]]);
    for (++ebml->off; n-- > 1; ebml->off++)
        v = (v << 8) | ebml->ptr[ebml->off];

    return v;
}

int32_t ebml_read_id(struct ebml_t* ebml)
{
    return (int32_t)ebml_read_int64(ebml, 4);
}

int64_t ebml_read_size(struct ebml_t* ebml)
{
    return ebml_read_int64(ebml, 8);
}
