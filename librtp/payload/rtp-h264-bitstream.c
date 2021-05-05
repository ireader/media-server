#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stddef.h>

#define RTP_H2645_BITSTREAM_FORMAT_DETECT 1

static const uint8_t* h264_startcode(const uint8_t* data, int bytes)
{
	int i;
	for (i = 2; i + 1 < bytes; i++)
	{
		if (0x01 == data[i] && 0x00 == data[i - 1] && 0x00 == data[i - 2])
			return data + i + 1;
	}

	return NULL;
}

/// @return >0-ok, <=0-error
static inline int h264_avcc_length(const uint8_t* h264, int bytes, int avcc)
{
	int i;
	uint32_t n;

	n = 0;
	assert(3 <= avcc && avcc <= 4);
	for (i = 0; i < avcc && i < bytes; i++)
		n = (n << 8) | h264[i];
	return avcc >= bytes ? -1 : (int)n;
}

/// @return 1-true, 0-false
static int h264_avcc_bitstream_valid(const uint8_t* h264, int bytes, int avcc)
{
	int n;

	while (avcc + 1 < bytes)
	{
		n = h264_avcc_length(h264, bytes, avcc);
		if (n < 0 || n + avcc > bytes)
			return 0; // invalid

		h264 += n + avcc;
		bytes -= n + avcc;
	}

	return 0 == bytes ? 1 : 0;
}

/// @return 0-annexb, >0-avcc, <0-error
static int h264_bitstream_format(const uint8_t* h264, int bytes)
{
	uint32_t n;
	if (bytes < 4)
		return -1;

	n = ((uint32_t)h264[0]) << 16 | ((uint32_t)h264[1]) << 8 | ((uint32_t)h264[2]);
	if (0 == n && h264[3] <= 1)
	{
		return 0; // annexb
	}
	else if (1 == n)
	{
		// try avcc & annexb
		return h264_avcc_bitstream_valid(h264, bytes, 4) ? 4 : 0;
	}
	else
	{
		// try avcc 4/3 bytes
		return h264_avcc_bitstream_valid(h264, bytes, 4) ? 4 : (h264_avcc_bitstream_valid(h264, bytes, 3) ? 3 : -1);
	}
}

static int h264_avcc_nalu(const void* h264, int bytes, int avcc, int (*handler)(void* param, const uint8_t* nalu, int bytes, int last), void* param)
{
	int r;
	uint32_t n;
	const uint8_t* p, * end;

	r = 0;
	p = (const uint8_t*)h264;
	end = (const uint8_t*)h264 + bytes;
	for (n = h264_avcc_length(p, (int)(end - p), avcc); 0 == r && p + n + avcc <= end; n = h264_avcc_length(p, (int)(end - p), avcc))
	{
		assert(n > 0);
		if (n > 0)
		{
			r = handler(param, p + avcc, (int)n, p + avcc + n < end ? 0 : 1);
		}

		p += n + avcc;
	}

	return r;
}

///@param[in] h264 H.264 byte stream format data(A set of NAL units)
int rtp_h264_annexb_nalu(const void* h264, int bytes, int (*handler)(void* param, const uint8_t* nalu, int bytes, int last), void* param)
{
	int r;
	ptrdiff_t n;
	const uint8_t* p, * next, * end;

#if defined(RTP_H2645_BITSTREAM_FORMAT_DETECT)
	int avcc;
	avcc = h264_bitstream_format(h264, bytes);
	if (avcc > 0)
		return h264_avcc_nalu(h264, bytes, avcc, handler, param);
#endif

	end = (const uint8_t*)h264 + bytes;
	p = h264_startcode((const uint8_t*)h264, bytes);

	r = 0;
	while (p && 0 == r)
	{
		next = h264_startcode(p, (int)(end - p));
		if (next)
		{
			n = next - p - 3;
		}
		else
		{
			n = end - p;
		}

		while (n > 0 && 0 == p[n - 1]) n--; // filter tailing zero

		assert(n > 0);
		if (n > 0)
		{
			r = handler(param, p, (int)n, next ? 0 : 1);
		}

		p = next;
	}

	return r;
}
