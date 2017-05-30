#ifndef _rtmp_client_transport_h_
#define _rtmp_client_transport_h_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct rtmp_client_transport_handler_t
{
	// play only
	void* (*alloc)(void* param, int avtype, size_t bytes);
	void (*onvideo)(void* param, const void* video, size_t bytes, uint32_t timestamp);
	void (*onaudio)(void* param, const void* audio, size_t bytes, uint32_t timestamp);

	// publish only
	void (*onready)(void* param);
	void (*onsend)(void* param);
};

void* rtmp_client_transport_create(const char* host, int port, const char* app, const char* stream, const char* tcurl, struct rtmp_client_transport_handler_t* handler, void* param);
void rtmp_client_transport_destroy(void* transport);

int rtmp_client_transport_start(void* transport, int publish);
int rtmp_client_transport_stop(void* transport);

int rtmp_client_transport_sendpacket(void* transport, int avtype, const void* flv, size_t bytes, uint32_t timestamp);

#ifdef __cplusplus
}
#endif
#endif /* !_rtmp_client_transport_h_ */
