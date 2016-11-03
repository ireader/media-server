#include "mpeg-types.h"
#include <assert.h>
#include <memory.h>

int find_h264_access_unit_delimiter(const uint8_t* p, size_t bytes)
{
	size_t i;
	uint8_t nalu;
	for (i = 2; i + 1 < bytes; i++)
	{
		if (0x01 == p[i] && 0x00 == p[i - 1] && 0x00 == p[i - 2])
		{
			nalu = p[i + 1] & 0x1f;
			if (9 == nalu || 5 == nalu || 1 == nalu)
				return 9 == nalu ? 1 : 0;
		}
	}

	return 0;
}

int find_h264_keyframe(const uint8_t* p, size_t bytes)
{
	size_t i;
	uint8_t nalu;
	for (i = 2; i + 1 < bytes; i++)
	{
		if (0x01 == p[i] && 0x00 == p[i - 1] && 0x00 == p[i - 2])
		{
			nalu = p[i + 1] & 0x1f;
			if (5 >= nalu && 1 <= nalu)
				return 5 == nalu ? 1 : 0;
		}
	}

	return 0;
}

// Apple MPEG-TS H.264 nalu stream
size_t mpeg_ts_h264(void* h264, size_t bytes)
{
	int i;
	int j = 0;
	uint8_t* p = (uint8_t*)h264;

	for (i = 1; i + 3 < (int)bytes; i++)
	{
		if (0x00 == p[i] && 0x00 == p[i + 1] && 0x01 == p[i + 2])
		{
			int nalu = p[i + 3] & 0x1f;
			if (7 != nalu && 8 != nalu && 9 != nalu)
			{ 
				for (j = i - 1; j >= 0 && 0x00 == p[j]; j--)
				{
				}

				if (++j < i)
				{
					memmove(p + j, p + i, bytes - i);
					bytes -= i - j;
					i = j;
				}
			}
		}
	}
	return bytes;
}
