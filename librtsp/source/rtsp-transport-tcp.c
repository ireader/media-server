#include "cstringext.h"
#include "rtsp-transport.h"
#include "tcpserver.h"
#include "rtsp-parser.h"
#include "aio-tcp-transport.h"

struct rtsp_tcp_session_t
{
	struct rtsp_tcp_transport_t *transport;
	void* session;
	void* parser; // rtsp parser
	char ip[32];
	int port;
	void* data;
};

struct rtsp_tcp_transport_t
{
	struct rtsp_transport_handler_t handler;
	void* ptr;
	void* transport;
};

static void* rtsp_transport_tcp_onconnected(void* ptr, void* sid, const char* ip, int port)
{
	struct rtsp_tcp_session_t *session;
	struct rtsp_tcp_transport_t *transport;
	transport = (struct rtsp_tcp_transport_t *)ptr;
	session = (struct rtsp_tcp_session_t *)malloc(sizeof(*session));
	memset(session, 0, sizeof(*session));
	session->parser = rtsp_parser_create(RTSP_PARSER_SERVER);
	session->session = sid;
	session->transport = transport;
	strncpy(session->ip, ip, sizeof(session->ip));
	session->port = port;
	return session;
}

static void rtsp_transport_tcp_ondisconnected(void* param)
{
	struct rtsp_tcp_session_t *session;
	struct rtsp_tcp_transport_t *transport;
	session = (struct rtsp_tcp_session_t *)param;
	transport = session->transport;
	rtsp_parser_destroy(session->parser);
#if defined(_DEBUG) || defined(DEBUG)
	memset(session, 0xCC, sizeof(*session));
#endif
	free(session);
}

static void rtsp_transport_tcp_onrecv(void* param, const void* msg, size_t bytes)
{
	int remain;
	struct rtsp_tcp_session_t *session;
	struct rtsp_tcp_transport_t *transport;
	session = (struct rtsp_tcp_session_t *)param;
	transport = session->transport;

	assert(bytes > 0);
	remain = bytes;
	if(0 == rtsp_parser_input(session->parser, msg, &remain))
	{
		// call
		// user must reply(send/send_vec/send_file) in handle
		aio_tcp_transport_addref(session->session);
		transport->handler.onrecv(transport->ptr, session->session, session->ip, session->port, session->parser, &session->data);

		rtsp_parser_clear(session->parser);
	}
	else
	{
		// wait more data
	}
}

static void rtsp_transport_tcp_onsend(void* param, int code, size_t bytes)
{
	struct rtsp_tcp_session_t *session;
	struct rtsp_tcp_transport_t *transport;
	session = (struct rtsp_tcp_session_t *)param;
	transport = session->transport;

	transport->handler.onsend(transport->ptr, session->data, code, bytes);
	aio_tcp_transport_release(session->session);
}

static void* rtsp_transport_tcp_create(socket_t socket, const struct rtsp_transport_handler_t *handler, void* ptr)
{
	struct rtsp_tcp_transport_t *transport;
	struct aio_tcp_transport_handler_t aiohandler;

	transport = (struct rtsp_tcp_transport_t*)malloc(sizeof(*transport));
	memset(transport, 0, sizeof(*transport));
	memcpy(&transport->handler, handler, sizeof(transport->handler));
	transport->ptr = ptr;

	memset(&aiohandler, 0, sizeof(aiohandler));
	aiohandler.onconnected = rtsp_transport_tcp_onconnected;
	aiohandler.ondisconnected = rtsp_transport_tcp_ondisconnected;
	aiohandler.onrecv = rtsp_transport_tcp_onrecv;
	aiohandler.onsend = rtsp_transport_tcp_onsend;
	transport->transport = aio_tcp_transport_create(socket, &aiohandler, transport);

	return transport;
}

static int rtsp_transport_tcp_destroy(void* t)
{
	struct rtsp_tcp_transport_t* transport;
	transport = (struct rtsp_tcp_transport_t*)t;
	aio_tcp_transport_destroy(transport->transport);
	return 0;
}

static int rtsp_transport_tcp_send(void* session, const void* msg, size_t bytes)
{
	int r;
	r = aio_tcp_transport_send(session, msg, bytes);
	if(0 != r)
		aio_tcp_transport_release(session);
	return r;
}

static int rtsp_transport_tcp_sendv(void* session, socket_bufvec_t *vec, int n)
{
	int r;
	r = aio_tcp_transport_sendv(session, vec, n);
	if(0 != r)
		aio_tcp_transport_release(session);
	return r;
}

struct rtsp_transport_t* rtsp_transport_tcp()
{
	static struct rtsp_transport_t transport;
	transport.create = rtsp_transport_tcp_create;
	transport.destroy = rtsp_transport_tcp_destroy;
	transport.send = rtsp_transport_tcp_send;
	transport.sendv = rtsp_transport_tcp_sendv;
	return &transport;
}
