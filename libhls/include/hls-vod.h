#ifndef _hls_vod_h_
#define _hls_vod_h_

#include "ctypedef.h"

#ifdef __cplusplus
extern "C" {
#endif

/// @param[in] pts ts file first pts(ms)
/// @param[in] duration file duration(ms)
/// @param[in] seq m3u8 sequence number(from 0)
/// @param[out] name saved file name
/// @param[in] namelen name size
typedef int (*hls_vod_handler)(void* param, const void* data, size_t bytes, int64_t pts, int64_t duration, uint64_t seq, char* name, size_t namelen);

/// param[in] duration ts file duration(millisecond), 0-defalut duration(10000s)
void* hls_vod_create(int64_t duration, hls_vod_handler handler, void* param);

void hls_vod_destroy(void* hls);

/// @param[in] pts present timestamp in millisecond
/// @param[in] dts decode timestamp in millisecond
/// @param[in] flags 1-force new segment
/// @return 0-ok, other-error
int hls_vod_input(void* hls, int avtype, const void* data, size_t bytes, int64_t pts, int64_t dts, int flags);

/// @return HLS segment count
size_t hls_vod_count(void* hls);

size_t hls_vod_m3u8(void* hls, char* m3u8, size_t bytes);

#ifdef __cplusplus
}
#endif
#endif /* !_hls_vod_h_ */

