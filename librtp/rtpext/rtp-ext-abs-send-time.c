#include "rtp-ext.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

// https://webrtc.googlesource.com/src/+/refs/heads/main/docs/native-code/rtp-hdrext/abs-send-time
/*
Wire format: 1-byte extension, 3 bytes of data. total 4 bytes extra per packet (plus shared 4 bytes for all extensions present: 2 byte magic word 0xBEDE, 2 byte # of extensions). Will in practice replace the ¡°toffset¡± extension so we should see no long term increase in traffic as a result.

Encoding: Timestamp is in seconds, 24 bit 6.18 fixed point, yielding 64s wraparound and 3.8us resolution (one increment for each 477 bytes going out on a 1Gbps interface).

Relation to NTP timestamps: abs_send_time_24 = (ntp_timestamp_64 >> 14) & 0x00ffffff ; NTP timestamp is 32 bits for whole seconds, 32 bits fraction of second.

SDP "a= name": "abs-send-time" ; this is also used in client/cloud signaling.
*/

int rtp_ext_abs_send_time_parse(const uint8_t* data, int bytes, uint64_t *timestamp)
{
    if (bytes < 3)
        return -1;

    *timestamp = ((uint64_t)data[0] << 16) | ((uint64_t)data[1] << 8) | data[2];
    *timestamp = (*timestamp * 1000000) >> 18;
    return 0;
}

int rtp_ext_abs_send_time_write(uint8_t* data, int bytes, uint64_t timestamp)
{
    if (bytes < 3)
        return -1;

    timestamp = (timestamp << 18) / 1000000;
    data[0] = (uint8_t)(timestamp >> 16);
    data[1] = (uint8_t)(timestamp >> 8);
    data[2] = (uint8_t)(timestamp >> 0);
    return 3;
}
