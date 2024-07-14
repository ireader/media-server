#ifndef _mpeg_util_h_
#define _mpeg_util_h_

#include "mpeg-types.h"
#include <assert.h>

struct mpeg_bits_t
{
	struct
	{
		const uint8_t* ptr;
		size_t len;
	} data[2];

	size_t count; // data count
	size_t len; // total length
	size_t off; // total offset
	int err; // flags
};

static inline void mpeg_bits_init(struct mpeg_bits_t* bits, const void* data, size_t bytes)
{
	bits->data[0].ptr = (const uint8_t*)data;
	bits->data[0].len = bytes;
	bits->count = 1;
	bits->len = bytes;
	bits->off = 0;
	bits->err = 0;
}

static inline void mpeg_bits_init2(struct mpeg_bits_t* bits, const void* data1, size_t bytes1, const void* data2, size_t bytes2)
{
	bits->data[0].ptr = (const uint8_t*)data1;
	bits->data[0].len = bytes1;
	bits->data[1].ptr = (const uint8_t*)data2;
	bits->data[1].len = bytes2;
	bits->len = bytes1 + bytes2;
	bits->count = 2;
	bits->off = 0;
	bits->err = 0;
}

/// @return 0-ok, other-error
static inline int mpeg_bits_error(struct mpeg_bits_t* bits)
{
	return bits->err;
}

static inline size_t mpeg_bits_length(struct mpeg_bits_t* bits)
{
	return bits->len;
}

static inline size_t mpeg_bits_tell(struct mpeg_bits_t* bits)
{
	return bits->off;
}

static inline void mpeg_bits_seek(struct mpeg_bits_t* bits, size_t n)
{
	bits->off = n;
	if (n > bits->len)
		bits->err = 1;
}

static inline void mpeg_bits_skip(struct mpeg_bits_t* bits, size_t n)
{
	bits->off += n;
	if (bits->off > bits->len)
		bits->err = 1;
}

static inline uint8_t mpeg_bits_read8(struct mpeg_bits_t* bits)
{
	size_t i, at;

	at = bits->off;
	for (i = 0; 0 == bits->err && i < bits->count; i++)
	{
		if (at < bits->data[i].len)
		{
			bits->off++;
			return bits->data[i].ptr[at];
		}
		at -= bits->data[i].len;
	}

	bits->err = 1;
	return 0;
}

static inline uint16_t mpeg_bits_read16(struct mpeg_bits_t* bits)
{
	uint16_t v;
	v = (uint16_t)mpeg_bits_read8(bits) << 8;
	v |= mpeg_bits_read8(bits);
	return v;
}

static inline uint32_t mpeg_bits_read32(struct mpeg_bits_t* bits)
{
	uint32_t v;
	v = ((uint32_t)mpeg_bits_read16(bits)) << 16;
	v |= mpeg_bits_read16(bits);
	return v;
}

static inline uint64_t mpeg_bits_read64(struct mpeg_bits_t* bits)
{
	uint64_t v;
	v = ((uint64_t)mpeg_bits_read32(bits)) << 32;
	v |= mpeg_bits_read32(bits);
	return v;
}

static inline uint64_t mpeg_bits_readn(struct mpeg_bits_t* bits, size_t n)
{
	size_t i;
	uint64_t v;

	for (v = i = 0; i < n && i < 8; i++)
		v = (v << 8) | mpeg_bits_read8(bits);
	return v;
}

static inline uint64_t mpeg_bits_tryread(struct mpeg_bits_t* bits, size_t n)
{
	uint64_t v;
	size_t i, offset;

	if (bits->err || bits->off + n > bits->len)
		return 0;
	
	offset = mpeg_bits_tell(bits);
	for (v = i = 0; i < n && i < 8; i++)
		v = (v << 8) | mpeg_bits_read8(bits);
	mpeg_bits_seek(bits, offset);

	return v;
}

static inline void nbo_w16(uint8_t* ptr, uint16_t val)
{
	ptr[0] = (uint8_t)((val >> 8) & 0xFF);
	ptr[1] = (uint8_t)(val & 0xFF);
}

static inline void nbo_w32(uint8_t* ptr, uint32_t val)
{
	ptr[0] = (uint8_t)((val >> 24) & 0xFF);
	ptr[1] = (uint8_t)((val >> 16) & 0xFF);
	ptr[2] = (uint8_t)((val >> 8) & 0xFF);
	ptr[3] = (uint8_t)(val & 0xFF);
}

void pcr_write(uint8_t *ptr, int64_t pcr);
int mpeg_stream_type_audio(int codecid);
int mpeg_stream_type_video(int codecid);

int mpeg_h264_find_nalu(const uint8_t* p, size_t bytes, size_t* leading);
int mpeg_h264_find_new_access_unit(const uint8_t* data, size_t bytes, int* vcl);
int mpeg_h265_find_new_access_unit(const uint8_t* data, size_t bytes, int* vcl);
int mpeg_h266_find_new_access_unit(const uint8_t* data, size_t bytes, int* vcl);
int mpeg_h26x_verify(const uint8_t* data, size_t bytes, int* codec);

int mpeg_h264_start_with_access_unit_delimiter(const uint8_t* p, size_t bytes);
int mpeg_h265_start_with_access_unit_delimiter(const uint8_t* p, size_t bytes);
int mpeg_h266_start_with_access_unit_delimiter(const uint8_t* p, size_t bytes);

uint32_t mpeg_crc32(uint32_t crc, const uint8_t *buffer, uint32_t size);

#endif /* !_mpeg_util_h_ */
