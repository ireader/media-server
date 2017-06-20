#include "mpeg-types.h"
#include "mpeg-util.h"
#include <assert.h>
#include <string.h>

int find_h265_access_unit_delimiter(const uint8_t* p, size_t bytes)
{
	size_t i;
	uint8_t type;
	for (i = 2; i + 1 < bytes; i++)
	{
		if (0x01 == p[i] && 0x00 == p[i - 1] && 0x00 == p[i - 2])
		{
			type = (p[i + 1] >> 1) & 0x3f;
			if (35 == type || type < 32)
				return 35 == type ? 1 : 0;
		}
	}

	return 0;
}

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
				return (16 <= type && type <= 21) ? 1 : 0;
		}
	}

	return 0;
}
