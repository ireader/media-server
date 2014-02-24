#include "rtsp-transport-udp.h"
#include "sys/sock.h"
#include "rtsp-parser.h"

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
	m_parser = rtsp_parser_create(RTSP_PARSER_SERVER);

	socket_t s = udpserver_create(ip, port);

	// maximum buffer size(waiting queue)
	socket_setrecvbuf(s, 64*1024); // set receive buffer

	m_socket = aio_socket_create(s, 1);

	int r = aio_socket_recvfrom(m_socket, m_buffer, sizeof(m_buffer), OnRecv, this);
	if(0 != r)
	{
		printf("RtspTransportUDP::RtspTransportUD start receive error: %d\n", r);
	}
}

RtspTransportUDP::~RtspTransportUDP()
{
	aio_socket_destroy(m_socket);

	if(m_parser)
	{
		rtsp_parser_destroy(m_parser);
		m_parser = NULL;
	}
}

void RtspTransportUDP::OnRecv(void* param, int code, int bytes, const char* ip, int port)
{
	RtspTransportUDP *session = (RtspTransportUDP*)param;
	session->OnRecv(code, bytes, ip, port);
}

void RtspTransportUDP::OnRecv(int code, int bytes, const char* ip, int port)
{
	if(0 == code && bytes > 0)
	{
		rtsp_parser_clear(m_parser);
		if(0 == rtsp_parser_input(m_parser, m_buffer, &bytes))
		{
			// callback
		}
	}

	// get next packet
	int r = aio_socket_recvfrom(m_socket, m_buffer, sizeof(m_buffer), OnRecv, this);
	if(0 != r)
	{
		printf("RtspTransportUDP::RtspTransportUD start receive error: %d\n", r);
	}
}
