#ifndef _mpeg_ps_h_
#define _mpeg_ps_h_
	
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

#ifdef LIBMPEG_EXPORTS
	#define LIBMPEG_API DLL_EXPORT_API
#else
	#define LIBMPEG_API DLL_IMPORT_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(OS_WINDOWS)
	typedef __int64 int64_t;
#else
	typedef long long int64_t;
#endif

enum
{
	STREAM_VIDEO_H264	= 0x1b,
	STREAM_VIDEO_SVAC	= 0x80,
	STREAM_AUDIO_AAC	= 0x0f,
	STREAM_AUDIO_SVAC	= 0x90,
};

typedef void (*mpeg_ps_cbwrite)(void* param, const void* packet, size_t bytes);

LIBMPEG_API void* mpeg_ps_create(mpeg_ps_cbwrite func, void* param);
LIBMPEG_API int mpeg_ps_destroy(void* ps);
LIBMPEG_API int mpeg_ps_add_stream(void* ps, int streamType, const void* info, int bytes);
LIBMPEG_API int mpeg_ps_write(void* ps, int streamType, int64_t pts, int64_t dts, const void* data, size_t bytes);
LIBMPEG_API int mpeg_ps_reset(void* ps);

LIBMPEG_API size_t mpeg_ps_packet_dec(const unsigned char* data, size_t bytes, mpeg_ps_cbwrite func, void* param);

#ifdef __cplusplus
}
#endif
#endif /* !_mpeg_ps_h_ */
