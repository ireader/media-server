#include "RtspSession.h"

RtspSession::RtspSession(socket_t sock, const char* ip, int port)
{
	m_ip = ip;
	m_port = port;
	m_socket = aio_socket_create(sock, 1);
}

RtspSession::~RtspSession()
{
	aio_socket_destroy(m_socket);
}

void RtspSession::Run()
{
	if(0 != aio_socket_recv(m_socket, m_rxbuffer, sizeof(m_rxbuffer)-1, OnRecv, this))
		release();
}

void RtspSession::Reply()
{
}

void RtspSession::OnRecv(void* param, int code, int bytes)
{
	RtspSession *self = (RtspSession *)param;
	if(0 == code && bytes > 0)
	{
		int remain = bytes;
		code = http_parser_input(self->m_http, self->m_buffer, &remain);
		if(0 == code)
		{
			self->OnApi();
		}
		else if(1 == code)
		{
			code = aio_socket_recv(self->m_socket, self->m_rxbuffer, sizeof(self->m_rxbuffer), OnRecv, self);
		}
	}

	if(code < 0 || 0 == bytes)
	{
		self->release();
	}
}

void RtspSession::OnSend(void* param, int code, int bytes)
{
	RtspSession *self = (RtspSession *)param;
	if(0 == code)
	{
		//http_parser_clear(self->m_http);
		code = aio_socket_recv(self->m_sock, self->m_buffer, sizeof(self->m_buffer), OnRecv, self);
	}

	if(code < 0)
	{
		self->release();
	}
}
