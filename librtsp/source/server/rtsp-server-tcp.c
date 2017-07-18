#include "rtsp-server-internal.h"
#include "aio-tcp-transport.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static void rtsp_transport_tcp_ondestroy(void* param)
{
	struct rtsp_session_t *session;
	session = (struct rtsp_session_t *)param;
	session->transport = NULL;

	if (session->parser)
	{
		rtsp_parser_destroy(session->parser);
		session->parser = NULL;
	}

#if defined(_DEBUG) || defined(DEBUG)
	memset(session, 0xCC, sizeof(*session));
#endif
	free(session);
}

static void rtsp_transport_tcp_onrecv(void* param, const void* data, size_t bytes)
{
	int r;
	int remain;
	struct rtsp_session_t *session;
	session = (struct rtsp_session_t *)param;

	remain = (int)bytes;
	do
	{
		r = rtsp_parser_input(session->parser, (char*)data + (bytes - remain), &remain);
		if (0 == r)
		{
			// call
			// user must reply(send/send_vec/send_file) in handle
			session->server->onrecv(session);

			rtsp_parser_clear(session->parser); // reset parser
		}
	} while (remain > 0 && r >= 0);
}

static void rtsp_transport_tcp_onsend(void* param, int code, size_t bytes)
{
	struct rtsp_session_t *session;
	session = (struct rtsp_session_t *)param;
	session->server->onsend(session, code, bytes);
}

static int rtsp_transport_tcp_send(void* transport, const void* data, size_t bytes)
{
	struct rtsp_session_t *session;
	session = (struct rtsp_session_t *)transport;
	return aio_tcp_transport_send(session->transport, data, bytes);
}

static int rtsp_transport_tcp_sendv(void* transport, socket_bufvec_t *vec, int n)
{
	struct rtsp_session_t *session;
	session = (struct rtsp_session_t *)transport;
	return aio_tcp_transport_sendv(session->transport, vec, n);
}

int rtsp_transport_tcp_create(socket_t socket, const struct sockaddr* addr, socklen_t addrlen, void* param)
{
	struct rtsp_session_t *session;
	struct aio_tcp_transport_handler_t handler;

	handler.ondestroy = rtsp_transport_tcp_ondestroy;
	handler.onrecv = rtsp_transport_tcp_onrecv;
	handler.onsend = rtsp_transport_tcp_onsend;

	session = (struct rtsp_session_t*)malloc(sizeof(*session));
	if (!session) return -1;

	assert(addrlen <= sizeof(session->addr));
	session->addrlen = addrlen < sizeof(session->addr) ? addrlen : sizeof(session->addr);
	memcpy(&session->addr, addr, session->addrlen);
	session->parser = rtsp_parser_create(RTSP_PARSER_SERVER);
	session->server = (struct rtsp_server_t *)param;
	session->sendv = rtsp_transport_tcp_sendv;
	session->send = rtsp_transport_tcp_send;
	session->transport = aio_tcp_transport_create(socket, &handler, session);

	if (NULL == session->transport)
	{
		free(session);
		return -1;
	}
	return 0;
}
