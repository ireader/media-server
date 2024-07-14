#include "rtp-ext.h"
#include "rtp-util.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

// https://webrtc.googlesource.com/src/+/refs/heads/main/docs/native-code/rtp-hdrext/abs-capture-time/
/*
RTP header extension format

Data layout overview
Data layout of the shortened version of abs-capture-time with a 1-byte header + 8 bytes of data:
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |  ID   | len=7 |     absolute capture timestamp (bit 0-23)     |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |             absolute capture timestamp (bit 24-55)            |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |  ... (56-63)  |
 +-+-+-+-+-+-+-+-+

Data layout of the extended version of abs-capture-time with a 1-byte header + 16 bytes of data:
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |  ID   | len=15|     absolute capture timestamp (bit 0-23)     |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |             absolute capture timestamp (bit 24-55)            |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |  ... (56-63)  |   estimated capture clock offset (bit 0-23)   |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |           estimated capture clock offset (bit 24-55)          |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |  ... (56-63)  |
 +-+-+-+-+-+-+-+-+

Data layout details

Absolute capture timestamp
Absolute capture timestamp is the NTP timestamp of when the first frame in a packet was originally captured. This timestamp MUST be based on the same clock as the clock used to generate NTP timestamps for RTCP sender reports on the capture system.
It's not always possible to do an NTP clock readout at the exact moment of when a media frame is captured. A capture system MAY postpone the readout until a more convenient time. A capture system SHOULD have known delays (e.g. from hardware buffers) subtracted from the readout to make the final timestamp as close to the actual capture time as possible.
This field is encoded as a 64-bit unsigned fixed-point number with the high 32 bits for the timestamp in seconds and low 32 bits for the fractional part. This is also known as the UQ32.32 format and is what the RTP specification defines as the canonical format to represent NTP timestamps.

Estimated capture clock offset
Estimated capture clock offset is the sender¡®s estimate of the offset between its own NTP clock and the capture system¡¯s NTP clock. The sender is here defined as the system that owns the NTP clock used to generate the NTP timestamps for the RTCP sender reports on this stream. The sender system is typically either the capture system or a mixer.
This field is encoded as a 64-bit two¡¯s complement signed fixed-point number with the high 32 bits for the seconds and low 32 bits for the fractional part. It¡¯s intended to make it easy for a receiver, that knows how to estimate the sender system¡¯s NTP clock, to also estimate the capture system¡¯s NTP clock:

 Capture NTP Clock = Sender NTP Clock + Capture Clock Offset

Further details

Capture system
A receiver MUST treat the first CSRC in the CSRC list of a received packet as if it belongs to the capture system. If the CSRC list is empty, then the receiver MUST treat the SSRC as if it belongs to the capture system. Mixers SHOULD put the most prominent CSRC as the first CSRC in a packet¡¯s CSRC list.

Intermediate systems
An intermediate system (e.g. mixer) MAY adjust these timestamps as needed. It MAY also choose to rewrite the timestamps completely, using its own NTP clock as reference clock, if it wants to present itself as a capture system for A/V-sync purposes.

Timestamp interpolation
A sender SHOULD save bandwidth by not sending abs-capture-time with every RTP packet. It SHOULD still send them at regular intervals (e.g. every second) to help mitigate the impact of clock drift and packet loss. Mixers SHOULD always send abs-capture-time with the first RTP packet after changing capture system.
A receiver SHOULD memorize the capture system (i.e. CSRC/SSRC), capture timestamp, and RTP timestamp of the most recently received abs-capture-time packet on each received stream. It can then use that information, in combination with RTP timestamps of packets without abs-capture-time, to extrapolate missing capture timestamps.
Timestamp interpolation works fine as long as there¡¯s reasonably low NTP/RTP clock drift. This is not always true. Senders that detect ¡°jumps¡± between its NTP and RTP clock mappings SHOULD send abs-capture-time with the first RTP packet after such a thing happening.
*/

int rtp_ext_absolute_capture_time_parse(const uint8_t* data, int bytes, struct rtp_ext_absolute_capture_time_t* ext)
{
    assert(bytes == 8 || bytes == 16);
    if (bytes != 8 && bytes != 16)
        return -1;

    memset(ext, 0, sizeof(*ext));
    ext->timestamp = rtp_read_uint64(data);
    ext->offset = bytes >= 16 ? rtp_read_uint64(data + 8) : 0;
    return 0;
}

int rtp_ext_absolute_capture_time_write(uint8_t* data, int bytes, const struct rtp_ext_absolute_capture_time_t* ext)
{
    if (bytes < 8)
        return -1;

    rtp_write_uint64(data, ext->timestamp);
    if (bytes >= 16)
        rtp_write_uint64(data + 8, ext->offset);
    return bytes >= 16 ? 16 : 8;
}
