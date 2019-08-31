#include "rtp-udp-transport.h"
#include "sockpair.h"
#include "ctypedef.h"
#include "port/ip-route.h"

RTPUdpTransport::RTPUdpTransport()
{
	m_socket[0] = socket_invalid;
	m_socket[1] = socket_invalid;
}

RTPUdpTransport::~RTPUdpTransport()
{
	for (int i = 0; i < 2; i++)
	{
		if (socket_invalid != m_socket[i])
			socket_close(m_socket[i]);
		m_socket[i] = socket_invalid;
	}
}

int RTPUdpTransport::Init(const char* ip, unsigned short port[2])
{
	char local[SOCKET_ADDRLEN];
	int r1 = socket_addr_from(&m_addr[0], &m_addrlen[0], ip, port[0]);
	int r2 = socket_addr_from(&m_addr[1], &m_addrlen[1], ip, port[1]);
	if (0 != r1 || 0 != r2)
		return 0 != r1 ? r1 : r2;

	r1 = ip_route_get(ip, local);
	return sockpair_create(0==r1 ? local : NULL, m_socket, port);
}

int RTPUdpTransport::Send(bool rtcp, const void* data, size_t bytes)
{
	int i = rtcp ? 1 : 0;
	return socket_sendto(m_socket[i], data, bytes, 0, (sockaddr*)&m_addr[i], m_addrlen[i]);
}
