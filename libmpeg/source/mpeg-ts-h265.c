#include "mpeg-types.h"
#include "mpeg-util.h"
#include <assert.h>
#include <string.h>

#define H265_NAL_AUD 35

/// @return -1-not found, other-AUD position(include start code)
int find_h265_access_unit_delimiter(const uint8_t* p, size_t bytes)
{
	size_t i;
	for (i = 2; i + 1 < bytes; i++)
	{
		if (0x01 == p[i] && 0x00 == p[i - 1] && 0x00 == p[i - 2] && H265_NAL_AUD == ((p[i + 1] >> 1) & 0x3f))
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

// Rec. ITU-T H.265 v4 (12/2016) (p26)
// intra random access point (IRAP) picture: 
//   A coded picture for which each VCL NAL unit has nal_unit_type 
//   in the range of BLA_W_LP to RSV_IRAP_VCL23, inclusive.
int find_h265_keyframe(const uint8_t* p, size_t bytes)
{
	size_t i;
	uint8_t type;
	for (i = 2; i + 1 < bytes; i++)
	{
		if (0x01 == p[i] && 0x00 == p[i - 1] && 0x00 == p[i - 2])
		{
			type = (p[i + 1] >> 1) & 0x3f;
			if (type < 32)
				return (16 <= type && type <= 23) ? 1 : 0;
		}
	}

	return 0;
}
