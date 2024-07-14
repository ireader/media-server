#include "rtp-ext.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

int rtp_ext_string_parse(const uint8_t* data, int bytes, char* v, int n)
{
    if (n < bytes + 1)
        return -1;

    memcpy(v, data, bytes);
    v[bytes] = 0;
    return 0;
}

int rtp_ext_string_write(uint8_t* data, int bytes, const char* v, int n)
{
    if (bytes < n)
        return -1;
    memcpy(data, v, n);
    return n;
}

// https://datatracker.ietf.org/doc/html/rfc8843#section-15.1
/*
       0                   1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |      MID=15   |     length    | identification-tag          ...
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
//int rtp_ext_sdes_mid_parse(const uint8_t* data, int bytes, const char* mid, int len)
//{
//	return rtp_ext_string_parse(data, bytes, mid, len);
//}


// https://datatracker.ietf.org/doc/html/rfc8852#section-4
// 1. RtpStreamId and RepairedRtpStreamId are limited to a total of 255 octets in length.
// 2. RtpStreamId and RepairedRtpStreamId are constrained to contain only alphanumeric characters.
//	  For avoidance of doubt, the only allowed byte values for these IDs are decimal 48 through 57, 65 through 90, and 97 through 122.
/*
        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |RtpStreamId=12 |     length    | RtpStreamId                 ...
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |Repaired...=13 |     length    | RepairRtpStreamId           ...
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
//int rtp_ext_sdes_rtp_stream_id(const uint8_t* data, int bytes, char* id, int len)
//{
//    return rtp_ext_string_parse(data, bytes, id, len);
//}
//
//int rtp_ext_sdes_repaired_rtp_stream_id(const uint8_t* data, int bytes, char* id, int len)
//{
//    return rtp_ext_string_parse(data, bytes, id, len);
//}
