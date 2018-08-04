#ifndef _mpeg_util_h_
#define _mpeg_util_h_

#include "mpeg-types.h"

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

int h264_find_nalu(const uint8_t* p, size_t bytes);
int find_h264_keyframe(const uint8_t* p, size_t bytes);
int find_h264_access_unit_delimiter(const uint8_t* p, size_t bytes);
int find_h265_keyframe(const uint8_t* p, size_t bytes);
int find_h265_access_unit_delimiter(const uint8_t* p, size_t bytes);

uint32_t mpeg_crc32(uint32_t crc, const uint8_t *buffer, uint32_t size);

#endif /* !_mpeg_util_h_ */
