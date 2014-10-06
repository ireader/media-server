#ifndef _rtsp_server_h_
#define _rtsp_server_h_

#include "rtsp-header-transport.h"

struct rtsp_handler_t
{
	int (*describe)(void* ptr, void* transport, const char* uri);
	int (*setup)(void* ptr, void* transport, const char* uri, const struct rtsp_header_transport_t* t);
	int (*play)(void* ptr, void* transport, const char* session, const int64_t *npt, const float *speed); // npt: 0 -> now
	int (*pause)(void* ptr, void* transport, const char* session, const int64_t *npt);
	int (*teardown)(void* ptr, void* transport, const char* session, const char* uri);
};

// Initialize/Finalize
// call once only
int rtsp_server_init();
int rtsp_server_cleanup();

/// start rtsp server
/// @param[in] ip bind ip address, NULL for any network interface
/// @param[in] port bind port, default 554
/// @return NULL-error, other-rtsp server instance
void* rtsp_server_create(const char* ip, int port, struct rtsp_handler_t* handler, void* ptr);

/// stop rtsp server
/// @param[in] server rtsp server instance
/// @return 0-ok, other-error code
int rtsp_server_destroy(void* rtsp);

int rtsp_server_report(void* rtsp);

int rtsp_server_reply_describe(void* transport, int code, const char* sdp);

int rtsp_server_reply_setup(void* transport, int code, const char* session);

/// Reply
/// @param[in] session handle callback session parameter
/// @param[in] code HTTP status-code(200-OK, 301-Move Permanently, ...)
/// @param[in] bundle create by http_bundle_alloc
/// @return 0-ok, other-error
int rtsp_server_reply_play(void* transport, int code, const char* session);

int rtsp_server_reply_pause(void* transport, int code, const char* session);

int rtsp_server_reply_teardown(void* transport, int code, const char* session);

#endif /* !_rtsp_server_h_ */
