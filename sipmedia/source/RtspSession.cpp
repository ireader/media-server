#include "RtspSession.h"
#include "rtsp-parser.h"

RtspSession::RtspSession(socket_t sock, const char* ip, int port)
{
	m_ip = ip;
	m_port = port;
	m_socket = aio_socket_create(sock, 1);
	m_rtsp = rtsp_parser_create(RTSP_PARSER_SERVER);
}

RtspSession::~RtspSession()
{
	rtsp_parser_destroy(m_rtsp);
	aio_socket_destroy(m_socket);
}

void RtspSession::Run()
{
	if(0 != aio_socket_recv(m_socket, m_rxbuffer, sizeof(m_rxbuffer)-1, OnRecv, this))
		release();
}

void RtspSession::OnApi()
{
	m_content = (void*)rtsp_get_content(m_http);
	m_contentLength = rtsp_get_content_length(m_http);
	if(m_contentLength > 0 && m_contentLength < 2*1024)
	{
		printf("%s\n", (const char*)m_content);
	}

	void* url = url_parse(rtsp_get_request_uri(m_http));
	m_path.assign(url_getpath(url));
	m_params.Init(url);
	url_free(url);
	printf("[%s] %s\n", m_ip.c_str(), m_path.c_str());

	typedef int (RtspSession::*Handler)();
	typedef std::map<std::string, Handler> THandlers;
	static THandlers handlers;
	if(0 == handlers.size())
	{
		handlers.insert(std::make_pair("proxy", &WebSession::OnProxy));
		handlers.insert(std::make_pair("comment", &WebSession::OnComment));
		handlers.insert(std::make_pair("cleanup", &WebSession::OnCleanup));
		handlers.insert(std::make_pair("addproxy", &WebSession::OnAddProxy));
		handlers.insert(std::make_pair("delproxy", &WebSession::OnDelProxy));
		handlers.insert(std::make_pair("listproxy", &WebSession::OnListProxy));
		handlers.insert(std::make_pair("hot-text", &WebSession::OnHotText));
		handlers.insert(std::make_pair("hot-image", &WebSession::OnHotImage));
		handlers.insert(std::make_pair("late-text", &WebSession::OnLateText));
		handlers.insert(std::make_pair("late-image", &WebSession::OnLateImage));
		handlers.insert(std::make_pair("18plus", &WebSession::On18Plus));
	}

	if(0 == strncmp(m_path.c_str(), "/api/", 5))
	{
		THandlers::iterator it;
		it = handlers.find(m_path.substr(5));
		if(it != handlers.end())
		{
			(this->*(it->second))();
			return;
		}
	}

	Reply(ERROR_NOTFOUND, "command not found");
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
		code = rtsp_parser_input(self->m_rtsp, self->m_rxbuffer, &remain);
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
		code = aio_socket_recv(self->m_socket, self->m_rxbuffer, sizeof(self->m_rxbuffer), OnRecv, self);
	}

	if(code < 0)
	{
		self->release();
	}
}
