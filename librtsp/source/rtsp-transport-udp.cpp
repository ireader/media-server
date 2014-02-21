#include "rtsp-transport-udp.h"
#include "sys/sock.h"

static socket_t udpserver_create(const char* ip, int port)
{
	int ret;
	socket_t socket;
	struct sockaddr_in addr;

	// new a UDP socket
	socket = socket_udp();
	if(socket_error == socket)
		return 0;

	// reuse addr
	socket_setreuseaddr(socket, 1);

	// bind
	if(ip && ip[0])
	{
		ret = socket_addr_ipv4(&addr, ip, (unsigned short)port);
		if(0 == ret)
			ret = socket_bind(socket, (struct sockaddr*)&addr, sizeof(addr));
	}
	else
	{
		ret = socket_bind_any(socket, (unsigned short)port);
	}

	if(0 != ret)
	{
		socket_close(socket);
		return 0;
	}

	return socket;
}

RtspTransportUDP::RtspTransportUDP(const char* ip, int port)
{
	socket_t s = udpserver_create(ip, port);

	m_socket = aio_socket_create(s, 1);
}

RtspTransportUDP::~RtspTransportUDP()
{
	aio_socket_destroy(m_socket);
}
