#ifndef _mpeg_util_h_
#define _mpeg_util_h_

#include "mpeg-types.h"

struct mpeg_bits_t
{
	const uint8_t* ptr;
	size_t len;
	size_t off;
};

static inline void mpeg_bits_init(struct mpeg_bits_t* bits, const void* data, size_t bytes)
{
	bits->ptr = (const uint8_t*)data;
	bits->len = bytes;
	bits->off = 0;
}

static inline int mpeg_bits_error(struct mpeg_bits_t* bits)
{
	return bits->off > bits->len ? 1 : 0;
}

static inline uint8_t mpeg_bits_read8(struct mpeg_bits_t* bits)
{
	return ++bits->off <= bits->len ? bits->ptr[bits->off - 1] : 0;
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
	v = (uint32_t)mpeg_bits_read16(bits) << 16;
	v |= mpeg_bits_read16(bits);
	return v;
}

static inline uint64_t mpeg_bits_read64(struct mpeg_bits_t* bits)
{
	uint64_t v;
	v = (uint64_t)mpeg_bits_read32(bits) << 32;
	v |= mpeg_bits_read32(bits);
	return v;
}

static inline void mpeg_bits_skip(struct mpeg_bits_t* bits, size_t n)
{
	bits->off += n;
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
int mpeg_h26x_verify(const uint8_t* data, size_t bytes, int* codec);

uint32_t mpeg_crc32(uint32_t crc, const uint8_t *buffer, uint32_t size);

#endif /* !_mpeg_util_h_ */
