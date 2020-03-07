#ifndef _hls_media_h_
#define _hls_media_h_

#include <stdint.h>
#include <stddef.h>

#define HLS_FLAGS_KEYFRAME 0x8000

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hls_media_t hls_media_t;

/// @param[in] param user-defined parameter(hls_media_create)
/// @param[in] data ts file content
/// @param[in] bytes ts file length in byte
/// @param[in] pts ts file first pts/dts(ms)
/// @param[in] dts ts file first pts/dts(ms)
/// @param[in] duration file duration(ms)
/// @return 0-ok, other-error
typedef int (*hls_media_handler)(void* param, const void* data, size_t bytes, int64_t pts, int64_t dts, int64_t duration);

/// param[in] duration ts segment duration(millisecond), 0-create segment per video key frame
hls_media_t* hls_media_create(int64_t duration, hls_media_handler handler, void* param);

void hls_media_destroy(hls_media_t* hls);

/// @param[in] avtype audio/video type (mpeg-ps.h STREAM_VIDEO_XXX/STREAM_AUDIO_XXX)
/// @param[in] data h264/h265 nalu with startcode(0x00000001), aac with adts
/// @param[in] bytes data length in byte, NULL-force new segment
/// @param[in] pts present timestamp in millisecond
/// @param[in] dts decode timestamp in millisecond
/// @param[in] flags HLS_FLAGS_XXX, such as HLS_FLAGS_KEYFRAME
/// @return 0-ok, other-error
int hls_media_input(hls_media_t* hls, int avtype, const void* data, size_t bytes, int64_t pts, int64_t dts, int flags);

#ifdef __cplusplus
}
#endif

#endif /* !_hls_media_h_*/
