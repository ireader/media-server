#ifndef _rtsp_server_aio_h_
#define _rtsp_server_aio_h_

#include "rtsp-server.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef void* aio_rtsp_connection_t;

struct aio_rtsp_handler_t
{
	struct rtsp_handler_t base;

	void (*onerror)(void* param, rtsp_server_t* rtsp, int code);
	void (*onrtp)(void* param, uint8_t channel, const void* data, uint16_t bytes);
};

void* rtsp_server_listen(const char* ip, int port, struct aio_rtsp_handler_t* handler, void* param);
int rtsp_server_unlisten(void* aio);

void* rtsp_transport_udp_create(const char* ip, int port, struct rtsp_handler_t* handler, void* param);
void rtsp_transport_udp_destroy(void* transport);

#if defined(__cplusplus)
}
#endif
#endif /* !_rtsp_server_aio_h_ */
