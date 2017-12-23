#ifndef _hls_h264_h_
#define _hls_h264_h_

#include <inttypes.h>
#include <stdint.h>
#include <stddef.h>

static inline const uint8_t* h264_startcode(const uint8_t *data, size_t bytes)
{
	size_t i;
	for (i = 2; i + 1 < bytes; i++)
	{
		if (0x01 == data[i] && 0x00 == data[i - 1] && 0x00 == data[i - 2])
			return data + i + 1;
	}

	return NULL;
}

static inline uint8_t h264_idr(const uint8_t *data, size_t bytes)
{
	uint8_t naltype;
	const uint8_t *p;

	do
	{
		p = h264_startcode(data, bytes);
		if (p)
		{
			naltype = p[0] & 0x1f;
			// 1: no-IDR slice
			// 2: A-slice
			// 3: B-slice
			// 4: C-slice
			// 5: IDR frame
			if (naltype > 0 && naltype < 6)
			{
				return 5 == naltype ? 1 : 0;
			}

			bytes -= p - data;
			data = p;
		}
	} while (p);

	return 0;
}

static inline int h265_irap(const uint8_t* p, size_t bytes)
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

#endif /* !_hls_h264_h_ */
