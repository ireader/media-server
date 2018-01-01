#ifndef _hls_m3u8_h_
#define _hls_m3u8_h_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hls_m3u8_t hls_m3u8_t;

///@param[in] live 1-live streaming, 0-vod
///@param[in] version m3u8 version 3-TS, 7-MP4
hls_m3u8_t* hls_m3u8_create(int live, int version);
void hls_m3u8_destroy(hls_m3u8_t* m3u8);

/// EXT-X-MAP
int hls_m3u8_set_x_map(hls_m3u8_t* m3u8, const char* name);

///@param[in] pts present timestamp (millisecond)
///@param[in] duration segment duration (millisecond)
///@param[in] discontinuity 1-EXT-X-DISCONTINUITY flag, 0-ignore
///@return 0-ok, other-error
int hls_m3u8_add(hls_m3u8_t* m3u8, const char* name, int64_t pts, int64_t duration, int discontinuity);

///@return media segment count
size_t hls_m3u8_count(hls_m3u8_t* m3u8);

///Get m3u8 playlist file
///@param[in] eof 1-EXT-X-ENDLIST, 0-ignore
///@return 0-ok, other-error
int hls_m3u8_playlist(hls_m3u8_t* m3u8, int eof, char* playlist, size_t bytes);

#ifdef __cplusplus
}
#endif
#endif /* !_hls_m3u8_h_ */
