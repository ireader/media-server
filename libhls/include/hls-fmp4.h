#ifndef _hls_fmp4_h_
#define _hls_fmp4_h_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hls_fmp4_t hls_fmp4_t;

/// @param[in] param user-defined parameter(hls_media_create)
/// @param[in] data ts file content
/// @param[in] bytes ts file length in byte
/// @param[in] pts ts file first pts/dts(ms)
/// @param[in] dts ts file first pts/dts(ms)
/// @param[in] duration file duration(ms)
/// @return 0-ok, other-error
typedef int (*hls_fmp4_handler)(void* param, const void* data, size_t bytes, int64_t pts, int64_t dts, int64_t duration);

/// @param[in] duration ts segment duration(millisecond), 0-create segment per video key frame
hls_fmp4_t* hls_fmp4_create(int64_t duration, hls_fmp4_handler handler, void* param);

void hls_fmp4_destroy(hls_fmp4_t* hls);

/// @param[in] object MPEG-4 systems ObjectTypeIndication such as: MOV_OBJECT_H264, see more @mov-format.h
/// @param[in] extra_data AudioSpecificConfig/AVCDecoderConfigurationRecord/HEVCDecoderConfigurationRecord
/// @return >=0-track, <0-error
int hls_fmp4_add_audio(hls_fmp4_t* hls, uint8_t object, int channel_count, int bits_per_sample, int sample_rate, const void* extra_data, size_t extra_data_size);
int hls_fmp4_add_video(hls_fmp4_t* hls, uint8_t object, int width, int height, const void* extra_data, size_t extra_data_size);

/// @param[in] track hls_fmp4_add_audio/hls_fmp4_add_video return value
/// @param[in] data h264/h265 mp4 format stream
/// @param[in] pts present timestamp in millisecond
/// @param[in] dts decode timestamp in millisecond
/// @param[in] flags MOV_AV_FLAG_XXX, such as: MOV_AV_FLAG_KEYFREAME, see more @mov-format.h
/// @return 0-ok, other-error
int hls_fmp4_input(hls_fmp4_t* hls, int track, const void* data, size_t bytes, int64_t pts, int64_t dts, int flags);

/// WARNING: hls_fmp4_input/hls_fmp4_init_segment not thread-safe
/// @param[in] data init segment buffer
/// @param[in] bytes data buffer size
/// @return >0-bytes, <0-error
int hls_fmp4_init_segment(hls_fmp4_t* hls, void* data, size_t bytes);

#ifdef __cplusplus
}
#endif

#endif /* !_hls_fmp4_h_ */
