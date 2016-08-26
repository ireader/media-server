#include "mpeg-types.h"
#include <assert.h>
#include "h264-sps.h"

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
			if (7 == nalu)
			{
				struct h264_sps_t sps;
				h264_parse_sps(p + i + 3, bytes, &sps);
			}
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
