#ifndef _hls_server_h_
#define _hls_server_h_

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	#define DLL_EXPORT_API __declspec(dllexport)
	#define DLL_IMPORT_API __declspec(dllimport)
#else
	#if __GNUC__ >= 4
		#define DLL_EXPORT_API __attribute__((visibility ("default")))
		#define DLL_IMPORT_API
	#else
		#define DLL_EXPORT_API
		#define DLL_IMPORT_API
	#endif
#endif

#ifdef LIBHLS_EXPORTS
	#define LIBHLS_API DLL_EXPORT_API
#else
	#define LIBHLS_API DLL_IMPORT_API
#endif


#ifdef __cplusplus
extern "C" {
#endif

LIBHLS_API int hls_server_init();
LIBHLS_API int hls_server_cleanup();

LIBHLS_API void* hls_server_create(const char* ip, int port);
LIBHLS_API int hls_server_destroy(void* hls);

typedef int (*hls_live_open)(void* param, void* camera, const char* id, const char* key, const char* publicId);
typedef int (*hls_live_close)(void* param, const char* id);
LIBHLS_API int hsl_server_set_handle(void* hls, hls_live_open open, hls_live_close close, void* param);

enum 
{
	STREAM_VIDEO_H264	= 0x1b,
	STREAM_AUDIO_AAC	= 0x0f,
};
LIBHLS_API int hsl_server_input(void* camera, const void* data, int bytes, int stream);

#ifdef __cplusplus
}
#endif
#endif /* !_hls_server_h_ */
