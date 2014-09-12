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

LIBMPEG_API size_t ps_packet_dec(const unsigned char* data, size_t bytes);

#ifdef __cplusplus
}
#endif
#endif /* !_mpeg_ps_h_ */
