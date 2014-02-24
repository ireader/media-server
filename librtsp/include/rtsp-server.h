#ifndef _rtsp_server_h_
#define _rtsp_server_h_

#include "rtsp-transport.h"

/// start rtsp server
/// @param[in] ip bind ip address, NULL for any network interface
/// @param[in] port bind port, default 554
/// @return NULL-error, other-rtsp server instance
void* rtsp_server_create();

/// stop rtsp server
/// @param[in] server rtsp server instance
/// @return 0-ok, other-error code
int rtsp_server_destroy(void* server);

int rtsp_server_add_transport(void* server, int transport);
int rtsp_server_delete_transport(void* server, int transport);

int rtsp_server_report(void* server);


#endif /* !_rtsp_server_h_ */
