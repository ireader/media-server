#ifndef _hls_string_h_
#define _hls_string_h_

#ifdef __cplusplus
extern "C" {
#endif

#if defined(OS_WINDOWS)
	#if !defined(strcasecmp)
		#define strcasecmp	_stricmp
	#endif
	#if !defined(strncasecmp)
		#define strncasecmp	_strnicmp
	#endif
#endif

size_t hls_base16_decode(void* target, const char* source, size_t bytes);

const char* hls_strtrim(const char* s, size_t* n, const char* prefix, const char* suffix);

size_t hls_strsplit(const char* ptr, const char* end, const char* delimiters, const char* quotes, const char** ppnext);

#ifdef __cplusplus
}
#endif

#endif /* !_hls_string_h_ */
