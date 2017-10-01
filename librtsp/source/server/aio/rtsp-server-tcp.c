#include "rtsp-server.h"
#include "aio-rwutil.h"
#include "sys/locker.h"
#include "sys/atomic.h"
#include "sockutil.h"
#include "aio-recv.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct rtsp_session_t
{
	int32_t ref;
	locker_t locker;
	char buffer[1024];

	struct rtsp_server_t *rtsp;
	struct sockaddr_storage addr;
	socklen_t addrlen;
	aio_socket_t socket;
	
	int rtimeout;
	int wtimeout;
	struct aio_recv_t recv;
	struct aio_socket_rw_t send;
};

static int rtsp_session_recv(struct rtsp_session_t *session);
static int rtsp_session_destroy(struct rtsp_session_t *session);

static void rtsp_session_release(struct rtsp_session_t *session)
{
	if (0 == atomic_decrement32(&session->ref))
	{
		assert(NULL == session->socket);
		
		if (session->rtsp)
		{
			rtsp_server_destroy(session->rtsp);
			session->rtsp = NULL;
		}

		locker_destroy(&session->locker);
#if defined(_DEBUG) || defined(DEBUG)
		memset(session, 0xCC, sizeof(*session));
#endif
		free(session);
	}
}

static void rtsp_session_ondestroy(void* param)
{
	struct rtsp_session_t *session;
	session = (struct rtsp_session_t *)param;
	assert(invalid_aio_socket == session->socket);
	rtsp_session_release(session);
}

static void rtsp_session_onrecv(void* param, int code, size_t bytes)
{
	int r;
	size_t remain;
	struct rtsp_session_t *session;
	session = (struct rtsp_session_t *)param;

	if (0 == code)
	{
		// call
		// user must reply(send/send_vec/send_file) in handle
		atomic_increment32(&session->ref);

		remain = bytes;
		r = rtsp_server_input(session->rtsp, session->buffer, &remain);
		assert(0 == remain); // FIXED ME: input multi-request
		if (1 == r)
		{
			// need more data
			atomic_decrement32(&session->ref);
		}
		else
		{
			code = r;
		}
	}

	if (0 == code)
	{
		// recv more data
		code = rtsp_session_recv(session);
	}

	// error or peer closed
	if (0 != code)
	{
		code = rtsp_session_destroy(session);
	}
}

static void rtsp_session_onsend(void* param, int code, size_t bytes)
{
	struct rtsp_session_t *session;
	session = (struct rtsp_session_t *)param;
//	session->server->onsend(session, code, bytes);
	if (0 != code)
		code = rtsp_session_destroy(session);
	rtsp_session_release(session);
	(void)bytes;
}

static int rtsp_session_destroy(struct rtsp_session_t *session)
{
	aio_socket_t socket;
	locker_lock(&session->locker);
	socket = session->socket;
	session->socket = invalid_aio_socket;
	locker_unlock(&session->locker);
	
	if (invalid_aio_socket == socket)
		return 0;
	return aio_socket_destroy(socket, rtsp_session_ondestroy, session);
}

static int rtsp_session_recv(struct rtsp_session_t *session)
{
	int r = -1;
	locker_lock(&session->locker);
	if (invalid_aio_socket != session->socket)
		r = aio_recv(&session->recv, session->rtimeout, session->socket, session->buffer, sizeof(session->buffer), rtsp_session_onrecv, session);
	locker_unlock(&session->locker);
	return r;
}

static int rtsp_session_send(void* ptr, const void* data, size_t bytes)
{
	int r = -1;
	struct rtsp_session_t *session;
	session = (struct rtsp_session_t *)ptr;
	locker_lock(&session->locker);
	if(invalid_aio_socket != session->socket)
		r = aio_socket_send_all(&session->send, session->wtimeout, session->socket, data, bytes, rtsp_session_onsend, session);
	locker_unlock(&session->locker);

	if (0 != r) rtsp_session_release(session);
	return r;
}

int rtsp_transport_tcp_create(socket_t socket, const struct sockaddr* addr, socklen_t addrlen, struct rtsp_handler_t* handler, void* param)
{
	char ip[65];
	unsigned short port;
	struct rtsp_session_t *session;
	struct rtsp_handler_t rtsphandler;

	memcpy(&rtsphandler, handler, sizeof(rtsphandler));
	rtsphandler.send = rtsp_session_send;

	session = (struct rtsp_session_t*)malloc(sizeof(*session));
	if (!session) return -1;

	session->ref = 1;
	session->rtimeout = 20000;
	session->wtimeout = 20000;
	locker_create(&session->locker);
	socket_addr_to(addr, addrlen, ip, &port);
	assert(addrlen <= sizeof(session->addr));
	session->addrlen = addrlen < sizeof(session->addr) ? addrlen : sizeof(session->addr);
	memcpy(&session->addr, addr, session->addrlen); // save client ip/port
	session->socket = aio_socket_create(socket, 1);
	session->rtsp = rtsp_server_create(ip, port, &rtsphandler, param, session); // reuse-able, don't need create in every link
	return aio_recv(&session->recv, session->rtimeout, session->socket, session->buffer, sizeof(session->buffer), rtsp_session_onrecv, session);
}
