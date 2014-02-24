#include "rtsp-transport-tcp.h"
#include "tcpserver.h"
#include "rtsp-parser.h"

#define MAX_BUFFER (1*1024)

std::string NewSessionId()
{
	return "";
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
		if(0 == code && bytes > 0)
		{
			bytes = session->OnProcess(bytes);

			bytes = session->Run();
		}
		else
		{
			// socket close ? 
			//RtspTransportTCP *transport = session->m_transport;
		}
	}

	int OnProcess(int bytes)
	{
		assert(bytes > 0);
		int r = rtsp_parser_input(m_parser, m_buffer, &bytes);
		if(0 == r)
		{
			// callback

			rtsp_parser_clear(m_parser);
		}

		return r;
	}

	int Run()
	{
		return aio_socket_recv(m_socket, m_buffer, sizeof(m_buffer), OnRecv, this);
	}

	const std::string& GetId() const
	{
		return m_id;
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
	pr = transport->m_sessions.insert(std::make_pair(session->GetId(), session));

	if(0 != session->Run())
	{
		transport->m_sessions.erase(pr.first);
		delete session;
	}
}
