#include "mpeg-types.h"
#include "mpeg-util.h"
#include <assert.h>
#include <string.h>

#define H264_NAL_IDR 5
#define H264_NAL_AUD 9

int h264_find_nalu(const uint8_t* p, size_t bytes)
{
    size_t i;
    for (i = 2; i + 1 < bytes; i++)
    {
        if (0x01 == p[i] && 0x00 == p[i - 1] && 0x00 == p[i - 2])
        {
            for (i -= 2; i > 0 && 0 == p[i - 1]; --i)
            {
                // filter trailing zero
            }
            return (int)i;
        }
    }

    return -1;
}

/// @return -1-not found, other-AUD position(include start code)
int find_h264_access_unit_delimiter(const uint8_t* p, size_t bytes)
{
	size_t i;
	for (i = 2; i + 1 < bytes; i++)
	{
		if (0x01 == p[i] && 0x00 == p[i - 1] && 0x00 == p[i - 2] && H264_NAL_AUD == (p[i + 1] & 0x1f))
		{
            for (i -= 2; i > 0 && 0 == p[i - 1]; --i)
            {
                // filter trailing zero
            }
            return (int)i;
		}
	}

	return -1;
}

int find_h264_keyframe(const uint8_t* p, size_t bytes)
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

