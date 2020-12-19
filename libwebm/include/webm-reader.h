#ifndef _webm_reader_h_
#define _webm_reader_h_

#include <stddef.h>
#include <stdint.h>
#include "webm-buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct webm_reader_t webm_reader_t;

webm_reader_t* webm_reader_create(const struct webm_buffer_t* buffer, void* param);
void webm_reader_destroy(webm_reader_t* webm);

struct webm_reader_trackinfo_t
{
	/// @param[in] object: MOV_OBJECT_H264/MOV_OBJECT_AAC, see more @webm-format.h
	void (*onvideo)(void* param, uint32_t track, uint8_t object, int width, int height, const void* extra, size_t bytes);
	void (*onaudio)(void* param, uint32_t track, uint8_t object, int channel_count, int bit_per_sample, int sample_rate, const void* extra, size_t bytes);
	void (*onsubtitle)(void* param, uint32_t track, uint8_t object, const void* extra, size_t bytes);
};

/// @return 0-OK, other-error
int webm_reader_getinfo(webm_reader_t* webm, struct webm_reader_trackinfo_t* ontrack, void* param);

uint64_t webm_reader_getduration(webm_reader_t* webm);

/// audio: AAC raw data, don't include ADTS/AudioSpecificConfig
/// video: 4-byte data length(don't include self length) + H.264 NALU(don't include 0x00000001)
/// @param[in] flags MOV_AV_FLAG_xxx, such as: MOV_AV_FLAG_KEYFREAME
typedef void (*webm_reader_onread)(void* param, uint32_t track, const void* buffer, size_t bytes, int64_t pts, int64_t dts, int flags);
/// @return 1-read one frame, 0-EOF, <0-error 
int webm_reader_read(webm_reader_t* webm, void* buffer, size_t bytes, webm_reader_onread onread, void* param);

/// @param[in,out] timestamp input seek timestamp, output seek location timestamp
/// @return 0-ok, other-error
int webm_reader_seek(webm_reader_t* webm, int64_t* timestamp);

#ifdef __cplusplus
}
#endif
#endif /* !_webm_reader_h_ */
