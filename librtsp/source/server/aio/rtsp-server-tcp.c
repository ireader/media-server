#include "rtsp-server-aio.h"
#include "aio-tcp-transport.h"
#include "sys/sock.h"
#include "sys/atomic.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#define TIMEOUT_RECV 20000
#define TIMEOUT_SEND 10000

struct rtsp_session_t
{
	uint64_t wclock; // last sent clock(for check recv timeout)
	aio_tcp_transport_t* aio;
	char buffer[1024];

	struct rtsp_server_t *rtsp;
	struct sockaddr_storage addr;
	socklen_t addrlen;

	void (*onerror)();
	void* param;
};

static void rtsp_session_ondestroy(void* param)
{
	struct rtsp_session_t *session;
	session = (struct rtsp_session_t *)param;

	// user call rtsp_server_destroy
	if (session->rtsp)
	{
		rtsp_server_destroy(session->rtsp);
		session->rtsp = NULL;
	}

#if defined(_DEBUG) || defined(DEBUG)
	memset(session, 0xCC, sizeof(*session));
#endif
	free(session);
}

static void rtsp_session_onrecv(void* param, int code, size_t bytes)
{
	size_t remain;
	struct rtsp_session_t *session;
	session = (struct rtsp_session_t *)param;

	if (0 == code)
	{
		remain = bytes;
		code = rtsp_server_input(session->rtsp, session->buffer, &remain);
		if (0 == code)
		{
			// TODO: pipeline remain data
			assert(bytes > remain);
			assert(0 == remain);
		}
		
		if (code >= 0)
		{
			// need more data
			code = aio_tcp_transport_recv(session->aio, session->buffer, sizeof(session->buffer));
		}
	}

	// error or peer closed
	if (0 != code || 0 == bytes)
	{
		session->onerror(session->param, session->rtsp, code ? code : ECONNRESET);
	}
}

static void rtsp_session_onsend(void* param, int code, size_t bytes)
{
	struct rtsp_session_t *session;
	session = (struct rtsp_session_t *)param;
//	session->server->onsend(session, code, bytes);
	if (0 != code)
		session->onerror(session->param, session->rtsp, code);
	(void)bytes;
}

static int rtsp_session_send(void* ptr, const void* data, size_t bytes)
{
	struct rtsp_session_t *session;
	session = (struct rtsp_session_t *)ptr;
	return aio_tcp_transport_send(session->aio, data, bytes);
}

static int rtsp_session_close(void* ptr)
{
	struct rtsp_session_t *session;
	session = (struct rtsp_session_t *)ptr;
	session->rtsp = NULL; // user call rtsp_server_destroy
	return aio_tcp_transport_destroy(session->aio);
}

int rtsp_transport_tcp_create(socket_t socket, const struct sockaddr* addr, socklen_t addrlen, struct aio_rtsp_handler_t* handler, void* param)
{
	char ip[65];
	unsigned short port;
	struct rtsp_session_t *session;
	struct rtsp_handler_t rtsphandler;
	struct aio_tcp_transport_handler_t h;

	memset(&h, 0, sizeof(h));
	h.ondestroy = rtsp_session_ondestroy;
	h.onrecv = rtsp_session_onrecv;
	h.onsend = rtsp_session_onsend;

	memcpy(&rtsphandler, &handler->base, sizeof(rtsphandler));
	rtsphandler.close = rtsp_session_close;
	rtsphandler.send = rtsp_session_send;

	session = (struct rtsp_session_t*)malloc(sizeof(*session));
	if (!session) return -1;

	socket_addr_to(addr, addrlen, ip, &port);
	assert(addrlen <= sizeof(session->addr));
	session->addrlen = addrlen < sizeof(session->addr) ? addrlen : sizeof(session->addr);
	memcpy(&session->addr, addr, session->addrlen); // save client ip/port

	session->param = param;
	session->onerror = handler->onerror;
	session->aio = aio_tcp_transport_create(socket, &h, session);
	session->rtsp = rtsp_server_create(ip, port, &rtsphandler, param, session); // reuse-able, don't need create in every link
	if (!session->rtsp || !session->aio)
	{
		rtsp_session_ondestroy(session);
		return -1;
	}
	
	aio_tcp_transport_set_timeout(session->aio, TIMEOUT_RECV, TIMEOUT_SEND);
	if (0 != aio_tcp_transport_recv(session->aio, session->buffer, sizeof(session->buffer)))
	{
		rtsp_session_ondestroy(session);
		return -1;
	}

	return 0;
}
