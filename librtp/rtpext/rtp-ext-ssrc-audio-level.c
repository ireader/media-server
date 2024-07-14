#include "rtp-ext.h"

// https://datatracker.ietf.org/doc/html/rfc6464
/*
   The audio level header extension carries the level of the audio in
   the RTP [RFC3550] payload of the packet with which it is associated.
   This information is carried in an RTP header extension element as
   defined by "A General Mechanism for RTP Header Extensions" [RFC5285].

   The payload of the audio level header extension element can be
   encoded using either the one-byte or two-byte header defined in
   [RFC5285].  Figures 1 and 2 show sample audio level encodings with
   each of these header formats.

                    0                   1
                    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
                   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                   |  ID   | len=0 |V| level       |
                   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

              Figure 1: Sample Audio Level Encoding Using the
                          One-Byte Header Format


      0                   1                   2                   3
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |      ID       |     len=1     |V|    level    |    0 (pad)    |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

              Figure 2: Sample Audio Level Encoding Using the
                          Two-Byte Header Format
*/

int rtp_ext_ssrc_audio_level_parse(const uint8_t* data, int bytes, uint8_t *activity, uint8_t *level)
{
    if (bytes < 1)
        return -1;

    *activity = (data[0] & 0x80) ? 1 : 0;
    *level = data[0] & 0x7f;
    return 0;
}

int rtp_ext_ssrc_audio_level_write(uint8_t* data, int bytes, uint8_t activity, uint8_t level)
{
    if (bytes < 1)
        return -1;

    data[0] = (activity ? 0x80 : 0) | level;
    return 1;
}
