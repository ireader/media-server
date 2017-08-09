#ifndef _rtsp_server_aio_h_
#define _rtsp_server_aio_h_

#include "rtsp-server.h"

#if defined(__cplusplus)
extern "C" {
#endif

void* rtsp_server_listen(const char* ip, int port, struct rtsp_handler_t* handler, void* param);
int rtsp_server_unlisten(void* aio);

void* rtsp_transport_udp_create(const char* ip, int port, struct rtsp_handler_t* handler, void* param);
void rtsp_transport_udp_destroy(void* transport);

#if defined(__cplusplus)
}
#endif
#endif /* !_rtsp_server_aio_h_ */
