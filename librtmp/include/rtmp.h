#ifndef _rtmp_h_
#define _rtmp_h_

#ifdef __cplusplus
extern "C" {
#endif

void* rtmp_create();

void rtmp_destroy(void** rtmp);

int rtmp_send_video(void* rtmp, const uint8_t* video);

#ifdef __cplusplus
}
#endif
#endif /* !_rtmp_h_ */
