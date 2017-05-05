#ifndef _hls_media_h_
#define _hls_media_h_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/// @param[in] pts ts file first pts(ms)
/// @param[in] duration file duration(ms)
/// @param[in] seq m3u8 sequence number(from 0)
/// @param[out] name saved file name
/// @param[in] namelen name size
typedef void (*hls_media_handler)(void* param, const void* data, size_t bytes, int64_t pts, int64_t dts, int64_t duration);

/// param[in] duration ts segment duration(millisecond), 0-create segment per video key frame
void* hls_media_create(int64_t duration, hls_media_handler handler, void* param);

void hls_media_destroy(void* hls);

/// @param[in] avtype audio/video type (mpeg-ps.h STREAM_VIDEO_XXX/STREAM_AUDIO_XXX)
/// @param[in] pts present timestamp in millisecond
/// @param[in] dts decode timestamp in millisecond
/// @param[in] flags 1-force new segment
/// @return 0-ok, other-error
int hls_media_input(void* hls, int avtype, const void* data, size_t bytes, int64_t pts, int64_t dts, int flags);

#ifdef __cplusplus
}
#endif

#endif /* !_hls_media_h_*/
