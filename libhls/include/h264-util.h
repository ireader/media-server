#ifndef _h264_util_h_
#define _h264_util_h_

#include <inttypes.h>
#include <stdint.h>
#include <stddef.h>

inline const uint8_t* h264_startcode(const uint8_t *data, size_t bytes)
{
	size_t i;
	for(i = 2; i < bytes; i++)
	{
		if(0x01 == data[i] && 0x00 == data[i-1] && 0x00 == data[i-2])
			return data + i + 1;
	}

	return NULL;
}

inline uint8_t h264_type(const uint8_t *data, size_t bytes)
{
	data = h264_startcode(data, bytes);
	return data ? (data[0] & 0x1f)  : 0x00;
}

inline uint8_t h264_idr(const uint8_t *data, size_t bytes)
{
	uint8_t naltype;
	const uint8_t *p;

	do
	{
		p = h264_startcode(data, bytes);
		if(p)
		{
			naltype = p[0] & 0x1f;
			// 1: no-IDR slice
			// 2: A-slice
			// 3: B-slice
			// 4: C-slice
			// 5: IDR frame
			if(naltype > 0 && naltype < 6)
			{
				return 5 == naltype ? 1 : 0;
			}

			bytes -= p - data;
			data = p;
		}
	} while(p);

	return 0;
}

#endif /* !_h264_util_h_ */
