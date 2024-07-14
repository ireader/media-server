#include "rtp-ext.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// https://datatracker.ietf.org/doc/html/draft-ietf-avtext-framemarking-13
/*
*   0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |  ID=? |  L=2  |S|E|I|D|B| TID |   LID         |    TL0PICIDX  |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
              or
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |  ID=? |  L=1  |S|E|I|D|B| TID |   LID         | (TL0PICIDX omitted)
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
              or
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |  ID=? |  L=0  |S|E|I|D|B| TID | (LID and TL0PICIDX omitted)
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/

int rtp_ext_frame_marking_parse(const uint8_t* data, int bytes, struct rtp_ext_frame_marking_t *ext)
{
    memset(ext, 0, sizeof(*ext));

    if (bytes-- > 0)
    {
        ext->s = (data[0] >> 7) & 0x01;
        ext->e = (data[0] >> 6) & 0x01;
        ext->i = (data[0] >> 5) & 0x01;
        ext->d = (data[0] >> 4) & 0x01;
        ext->b = (data[0] >> 3) & 0x01;
        ext->tid = data[0] & 0x07;
    }

    if (bytes-- > 0)
        ext->lid = data[1];

    if (bytes > 0)
        ext->tl0_pic_idx = data[2];

    return 0;
}

int rtp_ext_frame_marking_write(uint8_t* data, int bytes, const struct rtp_ext_frame_marking_t* ext)
{
    int len;

    len = 1 + ((ext->lid || ext->tl0_pic_idx) ? 1 : 0) + (ext->tl0_pic_idx ? 1 : 0);
    if (bytes < len)
        return -1;
    
    data[0] = ext->s ? 0x80 : 0x00;
    data[0] |= ext->e ? 0x40 : 0x00;
    data[0] |= ext->i ? 0x20 : 0x00;
    data[0] |= ext->d ? 0x10 : 0x00;
    data[0] |= ext->b ? 0x08 : 0x00;
    data[0] |= ext->tid;
    
    if (ext->lid || ext->tl0_pic_idx)
        data[1] = (uint8_t)ext->lid;

    if (ext->tl0_pic_idx)
        data[2] = (uint8_t)ext->tl0_pic_idx;

    return len;
}
