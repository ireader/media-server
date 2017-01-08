#ifndef _rtmp_netstream_h_
#define _rtmp_netstream_h_

int rtmp_netstream_play();
int rtmp_netstream_play2();
int rtmp_netstream_delete_stream();
int rtmp_netstream_receive_audio();
int rtmp_netstream_receive_video();
int rtmp_netstream_publish();
int rtmp_netstream_seek();
int rtmp_netstream_pause();

#endif /* !_rtmp_netstream_h_ */
