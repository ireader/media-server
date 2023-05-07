#include "mpeg-types.h"
#include "mpeg-util.h"
#include <assert.h>
#include <string.h>

#define H266_NAL_AUD 20

/// @return 0-not find, 1-find ok
int mpeg_h266_start_with_access_unit_delimiter(const uint8_t* p, size_t bytes)
{
    int i;
    uint8_t nalu;
    i = mpeg_h264_find_nalu(p, bytes, NULL);
    if (-1 == i)
        return 0;

    assert(i > 0);
    nalu = (p[i + 1] >> 3) & 0x1f;
    return H266_NAL_AUD == nalu ? 1 : 0;
}
