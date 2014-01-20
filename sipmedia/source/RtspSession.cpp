#include "sys/sock.h"
#include "cppstringext.h"
#include "RtspSession.h"
#include "rtsp-parser.h"
#include "rfc822-datetime.h"
#include "error.h"
#include "url.h"
#include <map>
#include <string>

struct comp
{
	bool operator()(const std::string& l, const std::string& r) const
	{
		return stricmp(l.c_str(), r.c_str()) < 0;
	}
};

typedef void (RtspSession::*Handler)();
typedef std::map<std::string, Handler, comp> THandlers;

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
	m_method = rtsp_get_request_method(m_rtsp);
	m_content = rtsp_get_content(m_rtsp);
	m_contentLength = rtsp_get_content_length(m_rtsp);
	if(m_contentLength > 0 && m_contentLength < 2*1024)
	{
		printf("%s\n", (const char*)m_content);
	}

	rtsp_get_header_by_name2(m_rtsp, "CSeq", &m_cseq);

	static THandlers handlers;
	if(0 == handlers.size())
	{
		handlers.insert(std::make_pair("Options", &RtspSession::Options));
		handlers.insert(std::make_pair("Describe", &RtspSession::Describe));
		handlers.insert(std::make_pair("Announce", &RtspSession::Announce));
		handlers.insert(std::make_pair("Setup", &RtspSession::Setup));
		handlers.insert(std::make_pair("Play", &RtspSession::Play));
		handlers.insert(std::make_pair("Pause", &RtspSession::Pause));
		handlers.insert(std::make_pair("Teardown", &RtspSession::Teardown));
		handlers.insert(std::make_pair("GetParameter", &RtspSession::GetParameter));
		handlers.insert(std::make_pair("SetParameter", &RtspSession::SetParameter));
		handlers.insert(std::make_pair("Redirect", &RtspSession::Redirect));
		handlers.insert(std::make_pair("Record", &RtspSession::Record));
		handlers.insert(std::make_pair("Embedded", &RtspSession::Embedded));
	}

	THandlers::iterator it;
	it = handlers.find(m_method);
	if(it == handlers.end())
	{
		Reply(ERROR_NOTFOUND, "command not found");
		return;
	}

	(this->*(it->second))();
}

void RtspSession::Reply(int code, const char* msg)
{
	sprintf(m_txbuffer, 
		"RTSP/1.0 %d %s\r\n"
		"CSeq: %u\r\n"
		"Content-Length: 0\r\n"
		"\r\n",
		code, msg);

	socket_setbufvec(m_vec, 0, m_txbuffer, strlen(m_txbuffer));
	aio_socket_send_v(m_socket, m_vec, 1, OnSend, this);
}

void RtspSession::Reply(const char* contentType, const void* data, int bytes)
{
	sprintf(m_txbuffer, 
		"RTSP/1.0 200 OK\r\n"
		"CSeq: %u\r\n"
		"Content-Type: %s\r\n"
		"Content-Length: %d\r\n"
		"\r\n",
		contentType, bytes);

	socket_setbufvec(m_vec, 0, m_txbuffer, strlen(m_txbuffer));
	socket_setbufvec(m_vec, 1, (void*)data, bytes);
	aio_socket_send_v(m_socket, m_vec, 2, OnSend, this);
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
		rtsp_parser_clear(self->m_rtsp);
		code = aio_socket_recv(self->m_socket, self->m_rxbuffer, sizeof(self->m_rxbuffer), OnRecv, self);
	}

	if(code < 0)
	{
		self->release();
	}
}

void RtspSession::Options()
{
	sprintf(m_txbuffer, 
		"RTSP/1.0 200 OK\r\n"
		"CSeq: %u\r\n"
		"Public: DESCRIBE, ANNOUNCE, SETUP, PLAY, PAUSE, TRARDOWN, GET_PARAMETER, SET_PARAMETER, REDIRECT, RECORD\r\n"
		"Content-Length: 0\r\n"
		"\r\n",
		m_cseq);

	aio_socket_send(m_socket, m_txbuffer, strlen(m_txbuffer), OnSend, this);
}

void RtspSession::Describe()
{
	void* url = url_parse(rtsp_get_request_uri(m_rtsp));
	const char* path = url_getpath(url);
	printf("[%s] %s\n", m_ip.c_str(), path);
	url_free(url);

	char date[27] = {0};
	datetime_format(time(NULL), date);

	sprintf(m_txbuffer, 
		"RTSP/1.0 200 OK\r\n"
		"CSeq: %u\r\n"
		"Date: %s\r\n"
		"Content-Type: application/sdp\r\n"
		"Content-Length: %d\r\n"
		"\r\n",
		m_cseq, date, bytes);

	sprintf(m_txbuffer,
		"v=0\r\n"
		"o=- %u %u IN IP4 %s\r\n"
		"s=RTSP Server\r\n"
		"i= \r\n"
		"c=IN IP4 %s\r\n"
		"t=%u %u\r\n"
		"a=recvonly\r\n"
		"m=audio %u RTP/AVP %u\r\n"
		"m=video %u RTP/AVP %u\r\n"
		GetClient().c_str());

	socket_setbufvec(m_vec, 0, m_txbuffer, strlen(m_txbuffer));
	socket_setbufvec(m_vec, 1, (void*)data, bytes);
	aio_socket_send_v(m_socket, m_vec, 2, OnSend, this);
}

void RtspSession::Announce()
{
}

void RtspSession::Setup()
{
}

void RtspSession::Play()
{
}

void RtspSession::Pause()
{
}

void RtspSession::Teardown()
{
}

void RtspSession::GetParameter()
{
}

void RtspSession::SetParameter()
{
}

void RtspSession::Redirect()
{
}

void RtspSession::Record()
{
}

void RtspSession::Embedded()
{
}

std::string RtspSession::GetClient() const
{
	std::string origin;

	// X-Forwarded-For: client1, proxy1, proxy2
	std::vector<std::string> clients;
	const char* xforwardfor = rtsp_get_header_by_name(m_rtsp, "X-Forward-For");
	if(xforwardfor)
	{
		Split(xforwardfor, ',', clients);
		origin = clients[0];
	}
	else
	{
		origin = m_ip;
	}

	return origin;
}
