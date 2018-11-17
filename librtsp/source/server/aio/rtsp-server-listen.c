#include "rtsp-server-aio.h"
#include "aio-accept.h"
#include "sockutil.h"
#include <stdlib.h>

struct rtsp_server_listen_t
{
	void* aio;
	void* param;
	struct aio_rtsp_handler_t handler;
};

extern int rtsp_transport_tcp_create(socket_t socket, const struct sockaddr* addr, socklen_t addrlen, struct aio_rtsp_handler_t* handler, void* param);

static void rtsp_server_onaccept(void* param, int code, socket_t socket, const struct sockaddr* addr, socklen_t addrlen)
{
	struct rtsp_server_listen_t* p;
	p = (struct rtsp_server_listen_t*)param;

	if (0 == code)
	{
		rtsp_transport_tcp_create(socket, addr, addrlen, &p->handler, p->param);
	}
	else
	{
		printf("http_server_onaccept code: %d\n", code);
	}
}

void* rtsp_server_listen(const char* ip, int port, struct aio_rtsp_handler_t* handler, void* param)
{
	socket_t socket;
	struct rtsp_server_listen_t* p;

	// create server socket
	socket = socket_tcp_listen(ip, (u_short)port, SOMAXCONN);
	if (socket_invalid == socket)
		return NULL;

	p = (struct rtsp_server_listen_t*)calloc(1, sizeof(*p));
	if (!p)
	{
		socket_close(socket);
		return NULL;
	}

	p->param = param;
	memcpy(&p->handler, handler, sizeof(p->handler));
	p->aio = aio_accept_start(socket, rtsp_server_onaccept, p);
	if (NULL == p->aio)
	{
		printf("rtsp_server_listen(%s, %d): start accept error.\n", ip, port);
		socket_close(socket);
		free(p);
		return NULL;
	}

	return p;
}

int rtsp_server_unlisten(void* aio)
{
	int r;
	struct rtsp_server_listen_t* p;
	p = (struct rtsp_server_listen_t*)aio;
	r = aio_accept_stop(p->aio, NULL, NULL);
	free(p);
	return r;
}
