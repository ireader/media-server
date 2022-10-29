#include "rtp-ext.h"
#include <string.h>
#include <assert.h>


// https://webrtc.googlesource.com/src/+/refs/heads/main/docs/native-code/rtp-hdrext/video-content-type
/*
SDP "a= name": "video-content-type" ; this is also used in client/cloud signaling.

Wire format: 1-byte extension, 1 bytes of data. total 2 bytes extra per packet (plus shared 4 bytes for all extensions present: 2 byte magic word 0xBEDE, 2 byte # of extensions).

    0                   1
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |  ID   | len=0 | Content type  |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

Values:
- 0x00: Unspecified. Default value. Treated the same as an absence of an extension.
- 0x01: Screenshare. Video stream is of a screenshare type.

Notes: Extension shoud be present only in the last packet of key-frames. If attached to other packets it should be ignored. If extension is absent, Unspecified value is assumed.
*/


int rtp_ext_video_content_type_parse(const uint8_t* data, int bytes, uint8_t *ext)
{
    assert(bytes == 1);
    *ext = bytes > 0 ? data[0] : 0;
    return 0;
}

int rtp_ext_video_content_type_write(uint8_t* data, int bytes, uint8_t ext)
{
    if (bytes < 1)
        return -1;

    data[0] = ext;
    return 1;
}
