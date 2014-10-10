#ifndef _hls_server_h_
#define _hls_server_h_

#ifdef __cplusplus
extern "C" {
#endif

int hls_server_init();
int hls_server_cleanup();

void* hls_server_create(const char* ip, int port);
int hls_server_destroy(void* hls);

typedef int (*hls_live_open)(void* param, void* camera, const char* id, const char* key, const char* publicId);
typedef int (*hls_live_close)(void* param, const char* id);
int hsl_server_set_handle(void* hls, hls_live_open open, hls_live_close close, void* param);

enum 
{
	HLS_VIDEO_H264	= 0x1b,
	HLS_AUDIO_AAC	= 0x0f,
};
int hsl_server_input(void* camera, const void* data, int bytes, int stream);

#ifdef __cplusplus
}
#endif
#endif /* !_hls_server_h_ */
