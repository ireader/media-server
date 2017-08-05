#ifndef _mov_reader_h_
#define _mov_reader_h_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mov_reader_t mov_reader_t;

mov_reader_t* mov_reader_create(const char* file);
void mov_reader_destroy(mov_reader_t* mov);

// @param[in] object: MOV_OBJECT_H264/MOV_OBJECT_AAC, see more @mov-format.h
typedef void (*mov_reader_onvideo)(void* param, uint8_t object, int width, int height, const void* extra, size_t bytes);
typedef void (*mov_reader_onaudio)(void* param, uint8_t object, int channel_count, int bit_per_sample, int sample_rate, const void* extra, size_t bytes);
int mov_reader_getinfo(mov_reader_t* mov, mov_reader_onvideo onvideo, mov_reader_onaudio onaudio, void* param);

// audio: AAC raw data, don't include ADTS/AudioSpecificConfig
// video: 4-byte data length(don't include self length) + H.264 NALU(don't include 0x00000001)
typedef void (*mov_reader_onread)(void* param, uint8_t object, const void* buffer, size_t bytes, int64_t pts, int64_t dts);
int mov_reader_read(mov_reader_t* mov, void* buffer, size_t bytes, mov_reader_onread onread, void* param);

#ifdef __cplusplus
}
#endif
#endif /* !_mov_reader_h_*/
