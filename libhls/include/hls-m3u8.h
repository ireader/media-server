#ifndef _hls_m3u8_h_
#define _hls_m3u8_h_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

///@param[in] live 1-live streaming, 0-vod
void* hls_m3u8_create(int live);
void hls_m3u8_destroy(void* m3u8);

///@param[in] pts present timestamp (millisecond)
///@param[in] duration segment duration (millisecond)
///@param[in] discontinuity 1-EXT-X-DISCONTINUITY flag, 0-ignore
///@return 0-ok, other-error
int hls_m3u8_add(void* m3u8, const char* name, int64_t pts, int64_t duration, int discontinuity);

///@return media segment count
size_t hls_m3u8_count(void* m3u8);

///Get m3u8 playlist file
///@param[in] eof 1-EXT-X-ENDLIST, 0-ignore
///@return 0-ok, other-error
int hls_m3u8_playlist(void* m3u8, int eof, char* playlist, size_t bytes);

#ifdef __cplusplus
}
#endif
#endif /* !_hls_m3u8_h_ */
