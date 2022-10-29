#include "rtp-ext.h"
#include "rtp-util.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

// https://webrtc.googlesource.com/src/+/refs/heads/main/docs/native-code/rtp-hdrext/transport-wide-cc-02/
/*
RTP header extension format
Data layout overview
Data layout of transport-wide sequence number 1-byte header + 2 bytes of data:

  0                   1                   2
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |  ID   | L=1   |transport-wide sequence number |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
Data layout of transport-wide sequence number and optional feedback request 1-byte header + 4 bytes of data:

  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |  ID   | L=3   |transport-wide sequence number |T|  seq count  |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |seq count cont.|
 +-+-+-+-+-+-+-+-+

Data layout details
The data is written in the following order,
- transport-wide sequence number (16-bit unsigned integer)
- feedback request (optional) (16-bit unsigned integer)
  If the extension contains two extra bytes for feedback request, this means that a feedback packet should be generated and sent immediately. The feedback request consists of a one-bit field giving the flag value T and a 15-bit field giving the sequence count as an unsigned number.
	- If the bit T is set the feedback packet must contain timing information.
	- seq count specifies how many packets of history that should be included in the feedback packet. If seq count is zero no feedback should be be generated, which is equivalent of sending the two-byte extension above. This is added as an option to allow for a fixed packet header size.
*/

int rtp_ext_transport_wide_cc_parse(const uint8_t* data, int bytes, struct rtp_ext_transport_wide_cc_t *ext)
{
    assert(bytes == 2 || bytes == 4);
    if (bytes < 2)
        return -1;
    
    memset(ext, 0, sizeof(*ext));
    ext->seq = rtp_read_uint16(data);
    if (bytes >= 4)
    {
        ext->t = (data[2] >> 7) & 0x01;
        ext->count = rtp_read_uint16(data + 2) & 0x7FFF;
    }
    return 0;
}

int rtp_ext_transport_wide_cc_write(uint8_t* data, int bytes, const struct rtp_ext_transport_wide_cc_t *ext)
{
    if (bytes < 2)
        return -1;

    rtp_write_uint16(data, (uint16_t)ext->seq);
    if (bytes >= 4)
        rtp_write_uint16(data + 2, (uint16_t)(ext->t << 15) | (ext->count & 0x7FFF));

    return bytes >= 4 ? 4 : 2;
}
