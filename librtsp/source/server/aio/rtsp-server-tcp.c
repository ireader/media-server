#include "rtsp-server.h"
#include "aio-tcp-transport.h"
#include "sys/locker.h"
#include "sys/atomic.h"
#include "sockutil.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct rtsp_session_t
{
	int32_t ref;
	locker_t locker;

	struct rtsp_server_t *rtsp;
	struct sockaddr_storage addr;
	socklen_t addrlen;
	void* transport;
};

static void rtsp_transport_tcp_release(struct rtsp_session_t *session)
{
	if (0 == atomic_decrement32(&session->ref))
	{
		assert(NULL == session->transport);
		
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

static void rtsp_transport_tcp_ondestroy(void* param)
{
	struct rtsp_session_t *session;
	session = (struct rtsp_session_t *)param;
	locker_lock(&session->locker);
	session->transport = NULL;
	locker_unlock(&session->locker);
	rtsp_transport_tcp_release(session);
}

static void rtsp_transport_tcp_onrecv(void* param, const void* data, size_t bytes)
{
	int r;
	struct rtsp_session_t *session;
	session = (struct rtsp_session_t *)param;

	// call
	// user must reply(send/send_vec/send_file) in handle
	atomic_increment32(&session->ref); // FIXED ME: input multi-request

	r = rtsp_server_input(session->rtsp, data, bytes);
}

static void rtsp_transport_tcp_onsend(void* param, int code, size_t bytes)
{
	struct rtsp_session_t *session;
	session = (struct rtsp_session_t *)param;
//	session->server->onsend(session, code, bytes);
	rtsp_transport_tcp_release(session);
}

static int rtsp_transport_tcp_send(void* ptr, const void* data, size_t bytes)
{
	int r = -1;
	struct rtsp_session_t *session;
	session = (struct rtsp_session_t *)ptr;
	locker_lock(&session->locker);
	if(NULL != session->transport)
		r = aio_tcp_transport_send(session->transport, data, bytes);
	locker_unlock(&session->locker);

	if (0 != r) rtsp_transport_tcp_release(session);
	return r;
}

int rtsp_transport_tcp_create(socket_t socket, const struct sockaddr* addr, socklen_t addrlen, struct rtsp_handler_t* handler, void* param)
{
	char ip[65];
	unsigned short port;
	struct rtsp_session_t *session;
	struct rtsp_handler_t rtsphandler;
	struct aio_tcp_transport_handler_t aiohandler;

	memcpy(&rtsphandler, handler, sizeof(rtsphandler));
	rtsphandler.send = rtsp_transport_tcp_send;

	aiohandler.ondestroy = rtsp_transport_tcp_ondestroy;
	aiohandler.onrecv = rtsp_transport_tcp_onrecv;
	aiohandler.onsend = rtsp_transport_tcp_onsend;

	session = (struct rtsp_session_t*)malloc(sizeof(*session));
	if (!session) return -1;

	session->ref = 1;
	locker_create(&session->locker);
	socket_addr_to(addr, addrlen, ip, &port);
	assert(addrlen <= sizeof(session->addr));
	session->addrlen = addrlen < sizeof(session->addr) ? addrlen : sizeof(session->addr);
	memcpy(&session->addr, addr, session->addrlen); // save client ip/port
	session->rtsp = rtsp_server_create(ip, port, &rtsphandler, param, session); // reuse-able, don't need create in every link
	session->transport = aio_tcp_transport_create(socket, &aiohandler, session);
	return aio_tcp_transport_start(session->transport);
}
