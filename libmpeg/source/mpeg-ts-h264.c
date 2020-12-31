#include "mpeg-types.h"
#include "mpeg-util.h"
#include <assert.h>
#include <string.h>

#define H264_NAL_IDR 5
#define H264_NAL_AUD 9

/// e.g. 
/// 1. 0x00 00 00 00 00 01 09 EF => return 6(09), leading 4
/// 2. 0x80 00 00 00 00 01 09 EF => return 6(09), leading 4
/// 
/// Find h264 nalu start position
/// @param[out] leading leading bytes before nalu position
/// @return -1-not found, other nalu position(after 00 00 01)
int mpeg_h264_find_nalu(const uint8_t* p, size_t bytes, size_t* leading)
{
    size_t i, zeros;
    for (zeros = i = 0; i + 1 < bytes; i++)
    {
        if (0x01 == p[i] && zeros >= 2)
        {
            assert(i >= zeros);
            if (leading)
                *leading = (zeros > 2 ? 3 : zeros) + 1; // zeros + 0x01
            return (int)(i + 1);
        }

        zeros = 0x00 != p[i] ? 0 : (zeros + 1);
    }

    return -1;
}

/// @param[out] leading optional leading zero bytes
/// @return -1-not found, other-AUD position(include start code)
static int mpeg_h264_find_access_unit_delimiter(const uint8_t* p, size_t bytes, size_t* leading)
{
    int i;
    size_t off;
    for (off = 0; off < bytes; off += i + 1)
	{
        i = mpeg_h264_find_nalu(p + off, bytes - off, leading);
        if (-1 == i)
            return -1;

        if (H264_NAL_AUD == (p[i + off] & 0x1f))
            return (int)(i + off);
	}

	return -1;
}

int mpeg_h264_find_keyframe(const uint8_t* p, size_t bytes)
{
	size_t i;
	uint8_t type;
	for (i = 2; i + 1 < bytes; i++)
	{
		if (0x01 == p[i] && 0x00 == p[i - 1] && 0x00 == p[i - 2])
		{
			type = p[i + 1] & 0x1f;
			if (H264_NAL_IDR >= type && 1 <= type)
				return H264_NAL_IDR == type ? 1 : 0;
		}
	}

	return 0;
}

/// h264_is_new_access_unit H.264 new access unit(frame)
/// @return 1-new access, 0-not a new access
static int mpeg_h264_is_new_access_unit(const uint8_t* nalu, size_t bytes)
{
    enum { NAL_NIDR = 1, NAL_PARTITION_A = 2, NAL_IDR = 5, NAL_SEI = 6, NAL_SPS = 7, NAL_PPS = 8, NAL_AUD = 9, };
    
    uint8_t nal_type;
    
    if(bytes < 2)
        return 0;
    
    nal_type = nalu[0] & 0x1f;
    
    // 7.4.1.2.3 Order of NAL units and coded pictures and association to access units
    if(NAL_AUD == nal_type || NAL_SPS == nal_type || NAL_PPS == nal_type || NAL_SEI == nal_type || (14 <= nal_type && nal_type <= 18))
        return 1;
    
    // 7.4.1.2.4 Detection of the first VCL NAL unit of a primary coded picture
    if(NAL_NIDR == nal_type || NAL_PARTITION_A == nal_type || NAL_IDR == nal_type)
    {
        // Live555 H264or5VideoStreamParser::parse
        // The high-order bit of the byte after the "nal_unit_header" tells us whether it's
        // the start of a new 'access unit' (and thus the current NAL unit ends an 'access unit'):
        return (nalu[1] & 0x80) ? 1 : 0; // first_mb_in_slice
    }
    
    return 0;
}

int mpeg_h264_find_new_access_unit(const uint8_t* data, size_t bytes, int* vcl)
{
    int n;
    size_t leading;
    uint8_t nal_type;
    const uint8_t* p, *end;

    end = data + bytes;
    for (p = data; p && p < end; p += n)
    {
        n = mpeg_h264_find_nalu(p, end - p, &leading);
        if (n < 0)
            return -1;

        nal_type = p[n] & 0x1f;
        if (*vcl > 0 && mpeg_h264_is_new_access_unit(p + n, end - p - n))
        {
            return p - data + n - leading;
        }
        else if (nal_type > 0 && nal_type < 6)
        {
            ++* vcl;
        }
        else
        {
            // nothing to do
        }
    }

    return -1;
}
