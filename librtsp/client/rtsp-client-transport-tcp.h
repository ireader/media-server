#ifndef _rtsp_client_transport_tcp_h_
#define _rtsp_client_transport_tcp_h_

#include "rtsp-client.h"

struct void* rtsp_client_connection_create();
void rtsp_client_connection_destroy(struct void* client);

#endif /* !_rtsp_client_transport_tcp_h_ */
