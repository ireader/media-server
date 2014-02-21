#include "rtsp-transport-tcp.h"
#include "tcpserver.h"

#define MAX_BUFFER (2*1024);

std::string NewSessionId()
{
}

class TransportSession
{
public:
	TransportSession(RtspTransportTCP *transport, socket_t socket, const char* ip, int port)
		:m_id(NewSessionId()), m_ip(ip), m_port(port), m_transport(transport)
	{
		m_socket = aio_socket_create(socket, 1);
		m_parser = rtsp_parser_create(RTSP_PARSER_SERVER);
	}

	~TransportSession()
	{
		if(invalid_aio_socket != m_socket)
		{
			aio_socket_destroy(m_socket);
			m_socket = invalid_aio_socket;
		}

		if(m_parser)
		{
			rtsp_parser_destroy(m_parser);
			m_parser = NULL;
		}
	}

	static void OnRecv(void* param, int code, int bytes)
	{
		TransportSession *session = (TransportSession*)param;
		RtspTransportTCP *transport = session->transport;
	}

	int Recv()
	{
		return aio_socket_recv(m_socket, m_buffer, sizeof(m_buffer), OnRecv, this);
	}

private:
	std::string m_id; // session id
	std::string m_ip;
	int m_port;
	aio_socket_t m_socket; // session socket

	char m_buffer[MAX_BUFFER];

	void *m_parser; // rtsp parser
	RtspTransportTCP *m_transport;
};

RtspTransportTCP::RtspTransportTCP(const char* ip, int port)
{
	socket_t s = tcpserver_create(ip, port, 128);

	m_socket = aio_socket_create(s, 1);
	aio_socket_accept(m_socket, OnAccept, this);
}

RtspTransportTCP::~RtspTransportTCP()
{
	aio_socket_destroy(m_socket);
}

void RtspTransportTCP::OnAccept(void* param, int code, socket_t socket, const char* ip, int port)
{
	RtspTransportTCP *transport = (RtspTransportTCP*)param;

	TransportSession *session = new TransportSession(transport, socket, ip, port);

	std::pair<TSessions::iterator, bool> pr;
	pr = transport->m_sessions.insert(std::make_pair(session->id, session));

	if(0 != session->Recv())
	{
		transport->m_sessions.erase(pr.first);
		delete session;
	}
}
