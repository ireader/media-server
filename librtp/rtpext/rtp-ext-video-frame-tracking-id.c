#include "rtp-ext.h"
#include "rtp-util.h"

// https://webrtc.googlesource.com/src/+/refs/heads/main/docs/native-code/rtp-hdrext/video-frame-tracking-id/
/*
Data layout overview
 1-byte header + 2 bytes of data:

  0                   1                   2
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |  ID   | L=1   |    video-frame-tracking-id    |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

 Notes: The extension shoud be present only in the first packet of each frame. If attached to other packets it can be ignored.
*/

int rtp_ext_video_frame_tracking_id_parse(const uint8_t* data, int bytes, uint16_t *id)
{
    if (bytes < 2)
        return -1;
    *id = rtp_read_uint16(data);
    return 0;
}

int rtp_ext_video_frame_tracking_id_write(uint8_t* data, int bytes, uint16_t id)
{
    if (bytes < 1)
        return -1;
    rtp_write_uint16(data, id);
    return 2;
}
