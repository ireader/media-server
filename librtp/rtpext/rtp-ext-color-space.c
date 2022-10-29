#include "rtp-ext.h"
#include "rtp-header.h"
#include "rtp-util.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

// https://webrtc.googlesource.com/src/+/refs/heads/main/docs/native-code/rtp-hdrext/color-space
/*
Data layout overview

Data layout without HDR metadata (one-byte RTP header extension) 1-byte header + 4 bytes of data:
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |  ID   | L = 3 |   primaries   |   transfer    |    matrix     |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |range+chr.sit. |
 +-+-+-+-+-+-+-+-+


Data layout of color space with HDR metadata (two-byte RTP header extension) 2-byte header + 28 bytes of data:
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |      ID       |   length=28   |   primaries   |   transfer    |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |    matrix     |range+chr.sit. |         luminance_max         |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |         luminance_min         |            mastering_metadata.|
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |primary_r.x and .y             |            mastering_metadata.|
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |primary_g.x and .y             |            mastering_metadata.|
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |primary_b.x and .y             |            mastering_metadata.|
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |white.x and .y                 |    max_content_light_level    |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 | max_frame_average_light_level |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+


Data layout details
The data is written in the following order, Color space information (4 bytes):
- Color primaries value according to ITU-T H.273 Table 2.
- Transfer characteristic value according to ITU-T H.273 Table 3.
- Matrix coefficients value according to ITU-T H.273 Table 4.
- Range and chroma siting as specified at https://www.webmproject.org/docs/container/#colour. Range (range), horizontal (horz) and vertical (vert) siting are merged to one byte by the operation: (range << 4) + (horz << 2) + vert.

The extension may optionally include HDR metadata written in the following order, Mastering metadata (20 bytes):
- Luminance max, specified in nits, where 1 nit = 1 cd/m2. (16-bit unsigned integer)
- Luminance min, scaled by a factor of 10000 and specified in the unit 1/10000 nits. (16-bit unsigned integer)
- CIE 1931 xy chromaticity coordinates of the primary red, scaled by a factor of 50000. (2x 16-bit unsigned integers)
- CIE 1931 xy chromaticity coordinates of the primary green, scaled by a factor of 50000. (2x 16-bit unsigned integers)
- CIE 1931 xy chromaticity coordinates of the primary blue, scaled by a factor of 50000. (2x 16-bit unsigned integers)
- CIE 1931 xy chromaticity coordinates of the white point, scaled by a factor of 50000. (2x 16-bit unsigned integers)

Followed by max light levels (4 bytes):
- Max content light level, specified in nits. (16-bit unsigned integer)
- Max frame average light level, specified in nits. (16-bit unsigned integer)


Note, the byte order for all integers is big endian.

See the standard SMPTE ST 2086 for more information about these entities.

Notes: Extension should be present only in the last packet of video frames. If attached to other packets it should be ignored.
*/

int rtp_ext_color_space_parse(const uint8_t* data, int bytes, struct rtp_ext_color_space_t* ext)
{
    assert(bytes == 4 || bytes == 28);
    if (bytes != 4 && bytes != 28)
        return -1;

    memset(ext, 0, sizeof(*ext));
    ext->primaries = data[0];
    ext->transfer = data[1];
    ext->matrix = data[2];
    ext->range_chroma_siting = data[3];

    if (28 == bytes)
    {
        ext->luminance_max = (((uint16_t)data[4]) << 8) | data[5];
        ext->luminance_min = (((uint16_t)data[6]) << 8) | data[7];
        ext->mastering_metadata_primary_red = rtp_read_uint32(data + 8);
        ext->mastering_metadata_primary_green = rtp_read_uint32(data + 12);
        ext->mastering_metadata_primary_blue = rtp_read_uint32(data + 16);
        ext->mastering_metadata_primary_white = rtp_read_uint32(data + 20);
        ext->max_content_light_level = rtp_read_uint16(data + 24);
        ext->max_frame_average_light_level = rtp_read_uint16(data + 26);
    }

    return 0;
}

int rtp_ext_color_space_write(uint8_t* data, int bytes, const struct rtp_ext_color_space_t* ext)
{
    if (bytes < 4)
        return -1;

    data[0] = ext->primaries;
    data[1] = ext->transfer;
    data[2] = ext->matrix;
    data[3] = ext->range_chroma_siting;

    if (bytes >= 28)
    {
        rtp_write_uint16(data + 4, ext->luminance_max);
        rtp_write_uint16(data + 6, ext->luminance_min);
        rtp_write_uint32(data + 8, ext->mastering_metadata_primary_red);
        rtp_write_uint32(data + 12, ext->mastering_metadata_primary_green);
        rtp_write_uint32(data + 16, ext->mastering_metadata_primary_blue);
        rtp_write_uint32(data + 20, ext->mastering_metadata_primary_white);
        rtp_write_uint16(data + 24, ext->max_content_light_level);
        rtp_write_uint16(data + 26, ext->max_frame_average_light_level);
    }

    return bytes >= 28 ? 28 : 4;
}
