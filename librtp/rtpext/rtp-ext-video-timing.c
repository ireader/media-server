#include "rtp-ext.h"
#include "rtp-util.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

// https://webrtc.googlesource.com/src/+/refs/heads/main/docs/native-code/rtp-hdrext/video-timing
/*
SDP "a= name": "video-timing" ; this is also used in client/cloud signaling.

Wire format: 1-byte extension, 13 bytes of data. Total 14 bytes extra per packet (plus 1-3 padding byte in some cases, plus shared 4 bytes for all extensions present: 2 byte magic word 0xBEDE, 2 byte # of extensions).

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |  ID   | len=12|     flags     |     encode start ms delta     |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |    encode finish ms delta     |  packetizer finish ms delta   |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |     pacer exit ms delta       |  network timestamp ms delta   |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |  network2 timestamp ms delta  |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

First byte is a flags field. Defined flags:
- 0x01 - extension is set due to timer.
- 0x02 - extension is set because the frame is larger than usual.
Both flags may be set at the same time. All remaining 6 bits are reserved and should be ignored.

Next, 6 timestamps are stored as 16-bit values in big-endian order, representing delta from the capture time of a packet in ms. Timestamps are, in order:
- Encode start.
- Encode finish.
- Packetization complete.
- Last packet left the pacer.
- Reserved for network.
- Reserved for network (2).

Pacer timestamp should be updated inside the RTP packet by pacer component when the last packet (containing the extension) is sent to the network. Last two, reserved timstamps, are not set by the sender but are reserved in packet for any in-network RTP stream processor to modify.

Notes: Extension shoud be present only in the last packet of video frames. If attached to other packets it should be ignored.
*/

int rtp_ext_video_timing_parse(const uint8_t* data, int bytes, struct rtp_ext_video_timing_t *ext)
{
    assert(bytes == 13);
    if (bytes < 13)
        return -1;

    memset(ext, 0, sizeof(*ext));
    ext->flags = data[0];
    ext->encode_start = rtp_read_uint16(data + 1);
    ext->encode_finish = rtp_read_uint16(data + 3);
    ext->packetization_complete = rtp_read_uint16(data + 5);
    ext->last_packet_left_the_pacer = rtp_read_uint16(data + 7);
    ext->network_timestamp = rtp_read_uint16(data + 9);
    ext->network_timestamp2 = rtp_read_uint16(data + 11);
    return 0;
}

int rtp_ext_video_timing_write(uint8_t* data, int bytes, const struct rtp_ext_video_timing_t *ext)
{
    if (bytes < 13)
        return -1;

    data[0] = (uint8_t)ext->flags;
    rtp_write_uint16(data + 1, ext->encode_start);
    rtp_write_uint16(data + 3, ext->encode_finish);
    rtp_write_uint16(data + 5, ext->packetization_complete);
    rtp_write_uint16(data + 7, ext->last_packet_left_the_pacer);
    rtp_write_uint16(data + 9, ext->network_timestamp);
    rtp_write_uint16(data + 11, ext->network_timestamp2);
    return 13;
}
