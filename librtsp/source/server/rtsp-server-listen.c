#include "sockutil.h"
#include "aio-accept.h"
#include "rtsp-server-internal.h"

static void rtsp_server_onaccept(void* param, int code, socket_t socket, const struct sockaddr* addr, socklen_t addrlen)
{
	if (0 == code)
	{
		rtsp_transport_tcp_create(socket, addr, addrlen, param);
	}
	else
	{
		printf("http_server_onaccept code: %d\n", code);
	}
}

void* rtsp_server_listen(const char* ip, int port, void* param)
{
	void* aio;
	socket_t socket;

	// create server socket
	socket = socket_tcp_listen(ip, (u_short)port, SOMAXCONN);
	if (socket_invalid == socket)
	{
		printf("rtsp_server_listen(%s, %d): create socket error.\n", ip, port);
		return NULL;
	}

	aio = aio_accept_start(socket, rtsp_server_onaccept, param);
	if (NULL == aio)
	{
		printf("rtsp_server_listen(%s, %d): start accept error.\n", ip, port);
		socket_close(socket);
		return NULL;
	}

	return aio;
}

int rtsp_server_unlisten(void* aio)
{
	return aio_accept_stop(aio, NULL, NULL);
}
