#ifndef _mkv_reader_h_
#define _mkv_reader_h_

#include <stddef.h>
#include <stdint.h>
#include "mkv-buffer.h"
#include "mkv-format.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mkv_reader_t mkv_reader_t;

mkv_reader_t* mkv_reader_create(const struct mkv_buffer_t* buffer, void* param);
void mkv_reader_destroy(mkv_reader_t* mkv);

struct mkv_reader_trackinfo_t
{
	/// @param[in] object: MKV_CODEC_VIDEO_H264/MKV_CODEC_AUDIO_AAC, see more @mkv-format.h
	void (*onvideo)(void* param, uint32_t track, enum mkv_codec_t codec, int width, int height, const void* extra, size_t bytes);
	void (*onaudio)(void* param, uint32_t track, enum mkv_codec_t codec, int channel_count, int bit_per_sample, int sample_rate, const void* extra, size_t bytes);
	void (*onsubtitle)(void* param, uint32_t track, enum mkv_codec_t codec, const void* extra, size_t bytes);
};

/// @return 0-OK, other-error
int mkv_reader_getinfo(mkv_reader_t* mkv, struct mkv_reader_trackinfo_t* ontrack, void* param);

uint64_t mkv_reader_getduration(mkv_reader_t* mkv);

/// audio: AAC raw data, don't include ADTS/AudioSpecificConfig
/// video: 4-byte data length(don't include self length) + H.264 NALU(don't include 0x00000001)
/// @param[in] flags MKV_FLAGS_xxx, such as: MKV_FLAGS_KEYFRAME
typedef void (*mkv_reader_onread)(void* param, uint32_t track, const void* buffer, size_t bytes, int64_t pts, int64_t dts, int flags);
/// @return 1-read one frame, 0-EOF, <0-error 
int mkv_reader_read(mkv_reader_t* mkv, void* buffer, size_t bytes, mkv_reader_onread onread, void* param);

/// audio: AAC raw data, don't include ADTS/AudioSpecificConfig
/// video: 4-byte data length(don't include self length) + H.264 NALU(don't include 0x00000001)
/// @param[in] flags MKV_FLAGS_xxx, such as: MKV_FLAGS_KEYFRAME
/// @return NULL-error, other-user alloc buffer
typedef void* (*mkv_reader_onread2)(void* param, uint32_t track, size_t bytes, int64_t pts, int64_t dts, int flags);
/// samp as mkv_reader_read + user alloc buffer
/// NOTICE: user should free buffer on return error!!!
/// @return 1-read one frame, 0-EOF, <0-error 
int mkv_reader_read2(mkv_reader_t* mkv, mkv_reader_onread2 onread, void* param);

/// @param[in,out] timestamp input seek timestamp, output seek location timestamp
/// @return 0-ok, other-error
int mkv_reader_seek(mkv_reader_t* mkv, int64_t* timestamp);

#ifdef __cplusplus
}
#endif
#endif /* !_mkv_reader_h_ */
