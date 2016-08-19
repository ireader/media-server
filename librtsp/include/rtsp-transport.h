#ifndef _rtsp_transport_h_
#define _rtsp_transport_h_

#include "sys/sock.h"

struct rtsp_transport_handler_t
{
	void (*onrecv)(void *ptr, void *session, const struct sockaddr* addr, socklen_t addrlen, void *parser, void **user);
	void (*onsend)(void *ptr, void *user, int code, size_t bytes);
};

struct rtsp_transport_t
{
	void* (*create)(socket_t socket, const struct rtsp_transport_handler_t *handler, void *ptr);
	int (*destroy)(void* transport);
	int (*send)(void *session, const void *msg, size_t bytes);
	int (*sendv)(void *session, socket_bufvec_t *vec, int n);
};

struct rtsp_transport_t* rtsp_transport_tcp();
struct rtsp_transport_t* rtsp_transport_udp();

#endif /* !_rtsp_transport_h_ */
