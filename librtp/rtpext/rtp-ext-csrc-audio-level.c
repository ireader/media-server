#include "rtp-ext.h"

// https://datatracker.ietf.org/doc/html/rfc6465
/*
   The audio level header extension carries the level of the audio in
   the RTP payload of the packet with which it is associated.  This
   information is carried in an RTP header extension element as defined
   by "A General Mechanism for RTP Header Extensions" [RFC5285].

   The payload of the audio level header extension element can be
   encoded using either the one-byte or two-byte header defined in
   [RFC5285].  Figures 2 and 3 show sample audio level encodings with
   each of these header formats.

       0                   1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |  ID   | len=2 |0|   level 1   |0|   level 2   |0|   level 3   |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

              Figure 2: Sample Audio Level Encoding Using the
                          One-Byte Header Format


       0                   1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |      ID       |     len=3     |0|   level 1   |0|   level 2   |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |0|   level 3   |    0 (pad)    |               ...
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

              Figure 3: Sample Audio Level Encoding Using the
                          Two-Byte Header Format
*/

int rtp_ext_csrc_audio_level_parse(const uint8_t* data, int bytes, uint8_t levels[], int num)
{
    int i;
    if (bytes < 1)
        return 0;

    for(i = 0; i < bytes && i < num; i++)
        levels[i] = data[0] & 0x7f;
    return i;
}

int rtp_ext_csrc_audio_level_write(uint8_t* data, int bytes, const uint8_t levels[], int num)
{
    int i;
    if (bytes < num)
        return -1;

    for(i = 0; i < num; i++)
        data[i] = levels[i] & 0x7f;
    return num;
}
