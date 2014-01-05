#ifndef _rfc_rtsp_h_
#define _rfc_rtsp_h_

int rtsp_options(void* rtsp);
int rtsp_describe(void* rtsp);
int rtsp_announce(void* rtsp);
int rtsp_setup(void* rtsp);
int rtsp_play(void* rtsp);
int rtsp_pause(void* rtsp);
int rtsp_teardown(void* rtsp);
int rtsp_get_parameter(void* rtsp);
int rtsp_set_parameter(void* rtsp);
int rtsp_redirect(void* rtsp);
int rtsp_record(void* rtsp);
int rtsp_embeded(void* rtsp);

#endif /* !_rfc_rtsp_h_ */
