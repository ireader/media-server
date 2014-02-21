#ifndef _rtsp_server_h_
#define _rtsp_server_h_

/// start rtsp server
/// @param[in] ip bind ip address, NULL for any network interface
/// @param[in] port bind port, default 554
/// @return NULL-error, other-rtsp server instance
void* rtsp_server_start(const char* ip, int port);

/// stop rtsp server
/// @param[in] server rtsp server instance
/// @return 0-ok, other-error code
int rtsp_server_stop(void* server);

int rtsp_server_report(void* server);

#endif /* !_rtsp_server_h_ */
