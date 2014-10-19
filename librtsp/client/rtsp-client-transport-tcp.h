#ifndef _rtsp_client_transport_tcp_h_
#define _rtsp_client_transport_tcp_h_

#if defined(__cplusplus)
extern "C" {
#endif

#include "rtsp-client.h"

typedef void (*rtsp_ondata)(void* ptr, int channel, const void* data, unsigned int bytes); // RTP over RTSP only(tcp mode)

void* rtsp_client_tcp_transport_create(rtsp_ondata ondata, void* ptr);
int rtsp_client_tcp_transport_destroy(void* transport);
int rtsp_client_tcp_transport_request(void* transport, const char* uri, const void* req, size_t bytes, void* rtsp, rtsp_onreply onreply);

#if defined(__cplusplus)
}
#endif
#endif /* !_rtsp_client_transport_tcp_h_ */
