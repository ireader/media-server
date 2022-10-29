#include "rtp-ext.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

// https://datatracker.ietf.org/doc/html/rfc5450
/*
       0                   1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |  ID   | len=2 |              transmission offset              |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
int rtp_ext_toffset_parse(const uint8_t* data, int bytes, uint32_t* timestamp)
{
    if (bytes < 3)
        return -1;

    *timestamp = ((uint32_t)data[0] << 16) | ((uint32_t)data[1] << 8) | data[2];
    return 0;
}

int rtp_ext_toffset_write(uint8_t* data, int bytes, uint32_t timestamp)
{
    if (bytes < 3)
        return -1;

    data[0] = (uint8_t)(timestamp >> 16);
    data[1] = (uint8_t)(timestamp >> 8);
    data[2] = (uint8_t)(timestamp >> 0);
    return 3;
}
