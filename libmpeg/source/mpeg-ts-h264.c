#include "mpeg-types.h"
#include "mpeg-util.h"
#include <assert.h>
#include <string.h>

int find_h264_access_unit_delimiter(const uint8_t* p, size_t bytes)
{
	size_t i;
	uint8_t type;
	for (i = 2; i + 1 < bytes; i++)
	{
		if (0x01 == p[i] && 0x00 == p[i - 1] && 0x00 == p[i - 2])
		{
			type = p[i + 1] & 0x1f;
			if (9 == type || (5 >= type && 1 <= type))
				return 9 == type ? 1 : 0;
		}
	}

	return 0;
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
			if (5 >= type && 1 <= type)
				return 5 == type ? 1 : 0;
		}
	}

	return 0;
}

