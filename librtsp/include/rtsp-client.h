#ifndef _rtsp_client_h_
#define _rtsp_client_h_

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

#if defined(LIBRTSP_EXPORTS)
	#define RTSP_CLIENT_API DLL_EXPORT_API
#else
	#define RTSP_CLIENT_API DLL_IMPORT_API
#endif

#if defined(__cplusplus)
extern "C" {
#endif

RTSP_CLIENT_API void* rtsp_open(const char* uri);
RTSP_CLIENT_API int rtsp_close(void* rtsp);
RTSP_CLIENT_API int rtsp_play(void* rtsp);
RTSP_CLIENT_API int rtsp_pause(void* rtsp);

#if defined(__cplusplus)
}
#endif

#endif /* !_rtsp_client_h_ */
