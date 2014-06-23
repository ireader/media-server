#ifndef _http_server_h_
#define _http_server_h_

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

#ifdef LIBHTTP_EXPORTS
	#define LIBHTTP_API DLL_EXPORT_API
#else
	#define LIBHTTP_API DLL_IMPORT_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct http_server_vec
{
	void* data;
	int bytes;
};

// Initialize/Finalize
// call once only
LIBHTTP_API int http_server_init();
LIBHTTP_API int http_server_cleanup();

LIBHTTP_API void* http_server_create(const char* ip, int port);
LIBHTTP_API int http_server_destroy(void* http);

// Options
LIBHTTP_API void http_server_set_timeout(void *server, int recv, int send);
LIBHTTP_API void http_server_get_timeout(void *server, int *recv, int *send);

// Request
LIBHTTP_API const char* http_server_get_header(void* session, const char *name);
LIBHTTP_API int http_server_get_content(void* session, void **content, int *length);

// Response
LIBHTTP_API int http_server_send(void* session, int code, const void* data, int bytes);
LIBHTTP_API int http_server_send_vec(void* session, int code, const struct http_server_vec* vec, int num);
LIBHTTP_API int http_server_set_header(void* session, const char* name, const char* value);
LIBHTTP_API int http_server_set_header_int(void* session, const char* name, int value);
LIBHTTP_API int http_server_set_content_type(void* session, const char* value);

// Handler
typedef int (*http_server_handler)(void* param, void* session, const char* method, const char* path);
LIBHTTP_API int http_server_set_handler(void* http, http_server_handler handler, void* param);


#ifdef __cplusplus
}
#endif
#endif /* !_http_server_h_ */
