#ifndef _mpeg_ts_h_
#define _mpeg_ts_h_

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

typedef void (*mpeg_ts_cbwrite)(void* param, const void* packet, size_t bytes);

LIBMPEG_API void* mpeg_ts_create(mpeg_ts_cbwrite func, void* param);

LIBMPEG_API int mpeg_ts_destroy(void* ts);

LIBMPEG_API int mpeg_ts_write(void* ts, int streamId, int64_t pts, int64_t dts, const void* data, size_t bytes);

LIBMPEG_API int mpeg_ts_reset(void* ts);

LIBMPEG_API int ts_packet_dec(const unsigned char* data, size_t bytes);

#ifdef __cplusplus
}
#endif
#endif /* !_mpeg_ts_h_ */
