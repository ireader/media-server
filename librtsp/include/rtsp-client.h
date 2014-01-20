#ifndef _rtsp_client_h_
#define _rtsp_client_h_

void* rtsp_open(const char* uri);
int rtsp_close(void* rtsp);
int rtsp_play(void* rtsp);
int rtsp_pause(void* rtsp);

#endif /* !_rtsp_client_h_ */
