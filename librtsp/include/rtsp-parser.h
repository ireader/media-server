#ifndef _rtsp_parser_h_
#define _rtsp_parser_h_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rtsp_parser_t rtsp_parser_t;

enum RTSP_PARSER_MODE { 
	RTSP_PARSER_CLIENT = 0, 
	RTSP_PARSER_SERVER = 1
};

/// get/set maximum body size(global setting)
/// @param[in] bytes 0-unlimited, other-limit bytes
int rtsp_get_max_size();
int rtsp_set_max_size(unsigned int bytes);

/// create
/// @param[in] mode 1-server mode, 0-client mode
/// @return parser instance
rtsp_parser_t* rtsp_parser_create(enum RTSP_PARSER_MODE mode);

/// destroy
/// @return 0-ok, other-error
int rtsp_parser_destroy(rtsp_parser_t* parser);

/// clear state
void rtsp_parser_clear(rtsp_parser_t* parser);

/// input data
/// @param[in] data content
/// @param[in/out] bytes out-remain bytes
/// @return 1-need more data, 0-receive done, <0-error
int rtsp_parser_input(rtsp_parser_t* parser, const void* data, int *bytes);

/// HTTP start-line
int rtsp_get_version(rtsp_parser_t* parser, int *major, int *minor);
int rtsp_get_status_code(rtsp_parser_t* parser);
const char* rtsp_get_status_reason(rtsp_parser_t* parser);
const char* rtsp_get_request_uri(rtsp_parser_t* parser);
const char* rtsp_get_request_method(rtsp_parser_t* parser);

/// HTTP body(use with rtsp_get_content_length)
const void* rtsp_get_content(rtsp_parser_t* parser);

/// HTTP headers
/// @return 0-ok, other-error
int rtsp_get_header_count(rtsp_parser_t* parser);
/// @return 0-ok, <0-don't have header
int rtsp_get_header(rtsp_parser_t* parser, int idx, const char** name, const char** value);
/// @return NULL-don't found header, other-header value
const char* rtsp_get_header_by_name(rtsp_parser_t* parser, const char* name);
/// @return 0-ok, <0-don't have header
int rtsp_get_header_by_name2(rtsp_parser_t* parser, const char* name, int *value);
/// @return >=0-content-length, <0-don't have content-length header
int rtsp_get_content_length(rtsp_parser_t* parser);
/// @return 1-close, 0-keep-alive, <0-don't have connection header
int rtsp_get_connection(rtsp_parser_t* parser);
/// @return Content-Encoding, 0-don't have this header
const char* rtsp_get_content_encoding(rtsp_parser_t* parser);
/// @return Transfer-Encoding, 0-don't have this header
const char* rtsp_get_transfer_encoding(rtsp_parser_t* parser);
/// @return Set-Cookie, 0-don't have this header
const char* rtsp_get_cookie(rtsp_parser_t* parser);
/// @return Location, 0-don't have this header
const char* rtsp_get_location(rtsp_parser_t* parser);

#ifdef __cplusplus
}
#endif
#endif /* !_rtsp_parser_h_ */
