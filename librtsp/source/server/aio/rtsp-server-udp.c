#include "rtsp-server-aio.h"
#include "aio-socket.h"
#include "sys/atomic.h"
#include "sys/locker.h"
#include "sockutil.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct rtsp_udp_transport_t
{
	int32_t ref;
	int running;
	socket_t socket;
	aio_socket_t aio;

	size_t size; // udp buffer size

	struct rtsp_handler_t handler;
	void* param;
};

struct rtsp_udp_session_t
{
	struct rtsp_udp_transport_t* transport;
	struct rtsp_server_t *rtsp;
	struct sockaddr_storage addr;
	socklen_t addrlen;
};

static void rtsp_transport_udp_release(struct rtsp_udp_transport_t* t);
static void rtsp_transport_udp_recv(struct rtsp_udp_transport_t* t);
static void rtsp_transport_udp_onrecv(void* param, int code, size_t bytes, const struct sockaddr* addr, socklen_t addrlen);
static int rtsp_transport_udp_send(void* param, const void* data, size_t bytes); 

void* rtsp_transport_udp_create(const char* ip, int port, struct rtsp_handler_t* handler, void* param)
{
	struct rtsp_udp_transport_t* t;
	t = (struct rtsp_udp_transport_t*)calloc(1, sizeof(*t));
	if (t)
	{
		t->ref = 1;
		t->size = 4 * 1024; // 2k
		t->running = 1;
		t->param = param;
		memcpy(&t->handler, handler, sizeof(t->handler));
		handler->send = rtsp_transport_udp_send;

		t->socket = socket_udp_bind(ip, (u_short)port);
		if (socket_invalid == t->socket)
		{
			free(t);
			return NULL;
		}

		t->aio = aio_socket_create(t->socket, 1);

		rtsp_transport_udp_recv(t);
	}

	return t;
}

void rtsp_transport_udp_destroy(void* transport)
{
	struct rtsp_udp_transport_t* t;
	t = (struct rtsp_udp_transport_t*)transport;
	t->running = 0;
	rtsp_transport_udp_release(t);
}

static struct rtsp_udp_session_t* rtsp_udp_session_create(struct rtsp_udp_transport_t* t)
{
	struct rtsp_udp_session_t* session;
	session = (struct rtsp_udp_session_t*)malloc(sizeof(*session) + t->size /*udp recv buffer*/);
	if (session)
	{
		atomic_increment32(&t->ref);
		session->transport = t;
	}
	return session;
}

static void rtsp_udp_session_destroy(struct rtsp_udp_session_t* session)
{
	struct rtsp_udp_transport_t* transport;
	transport = (struct rtsp_udp_transport_t*)session->transport;
	rtsp_transport_udp_release(transport);

	// TODO: reuse rtsp server
	if (session->rtsp)
	{
		rtsp_server_destroy(session->rtsp);
		session->rtsp = NULL;
	}

	free(session);
}

static void rtsp_transport_udp_ondestroy(void* param)
{
	struct rtsp_udp_transport_t* t;
	t = (struct rtsp_udp_transport_t*)param;
	free(t);
}

static void rtsp_transport_udp_release(struct rtsp_udp_transport_t* t)
{
	if (0 == atomic_decrement32(&t->ref))
	{
		assert(invalid_aio_socket != t->aio);
		aio_socket_destroy(t->aio, rtsp_transport_udp_ondestroy, t);
	}
}

static void rtsp_transport_udp_recv(struct rtsp_udp_transport_t* t)
{
	void* buffer;
	struct rtsp_udp_session_t* session;
	session = rtsp_udp_session_create(t);
	if (session)
	{
		buffer = session + 1;
		if (!t->running || 0 != aio_socket_recvfrom(t->aio, buffer, t->size, rtsp_transport_udp_onrecv, session))
			rtsp_udp_session_destroy(session);
	}
	else
	{
		// do nothing
	}
}

static void rtsp_transport_udp_onrecv(void* param, int code, size_t bytes, const struct sockaddr* addr, socklen_t addrlen)
{
	char ip[65];
	unsigned short port;
	struct rtsp_udp_session_t* session;
	struct rtsp_udp_transport_t* transport;
	session = (struct rtsp_udp_session_t*)param;
	transport = (struct rtsp_udp_transport_t*)session->transport;

	if (0 == code && bytes > 0)
	{
		size_t remain = bytes;
		rtsp_transport_udp_recv(transport); //  recv more

		socket_addr_to(addr, addrlen, ip, &port);
		assert(addrlen <= sizeof(session->addr));
		session->addrlen = addrlen < sizeof(session->addr) ? addrlen : sizeof(session->addr);
		memcpy(&session->addr, addr, session->addrlen); // save client ip/port
		session->rtsp = rtsp_server_create(ip, port, &session->transport->handler, session->transport->param, session);
		code = rtsp_server_input(session->rtsp, session + 1, &remain);
	}

	if (0 != code || bytes < 1)
	{
		assert(0);
		rtsp_udp_session_destroy(session);
	}
}

static void rtsp_transport_udp_onsend(void* param, int code, size_t bytes)
{
	struct rtsp_udp_session_t* session;
	session = (struct rtsp_udp_session_t*)param;
//	session->server->onsend(session, code, bytes);
	rtsp_udp_session_destroy(session);
}

static int rtsp_transport_udp_send(void* param, const void* data, size_t bytes)
{
	int r;
	struct rtsp_udp_session_t* session;
	struct rtsp_udp_transport_t* transport;
	session = (struct rtsp_udp_session_t*)param;
	transport = (struct rtsp_udp_transport_t*)session->transport;
	if (transport->running)
		r = socket_sendto(transport->socket, data, bytes, 0, (struct sockaddr*)&session->addr, session->addrlen);
	else
		r = -1;
	rtsp_transport_udp_onsend(session, r, r);
	return 0;
}
