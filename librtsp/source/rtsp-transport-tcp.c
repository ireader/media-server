#include "cstringext.h"
#include "rtsp-transport.h"
#include "sys/atomic.h"
#include "sys/locker.h"
#include "tcpserver.h"
#include "rtsp-parser.h"
#include "aio-tcp-transport.h"

struct rtsp_tcp_session_t
{
    int32_t ref;
    locker_t locker;
	void* session;
	void* parser; // rtsp parser
	struct sockaddr_storage addr;
	socklen_t addrlen;
	void* data;
    struct rtsp_transport_handler_t handler;
    void* ptr;
};

struct rtsp_tcp_transport_t
{
	struct rtsp_transport_handler_t handler;
	void* ptr;
	void* transport;
};

static void rtsp_transport_tcp_session_release(struct rtsp_tcp_session_t *session)
{
    if(0 == atomic_decrement32(&session->ref))
    {
        locker_destroy(&session->locker);
        rtsp_parser_destroy(session->parser);
    #if defined(_DEBUG) || defined(DEBUG)
        memset(session, 0xCC, sizeof(*session));
    #endif
        free(session);
    }
}

static void* rtsp_transport_tcp_onconnected(void* ptr, void* sid, const struct sockaddr* sa, socklen_t salen)
{
	struct rtsp_tcp_session_t *session;
	struct rtsp_tcp_transport_t *transport;
	transport = (struct rtsp_tcp_transport_t *)ptr;

	session = (struct rtsp_tcp_session_t *)malloc(sizeof(*session));
	if(!session) return NULL;

	memset(session, 0, sizeof(*session));
    memcpy(&session->handler, &transport->handler, sizeof(session->handler));
    session->ptr = transport->ptr;
    session->ref = 1;
    locker_create(&session->locker);
	session->parser = rtsp_parser_create(RTSP_PARSER_SERVER);
	session->session = sid;
	assert(salen < sizeof(session->addr));
	memcpy(&session->addr, sa, salen);
	session->addrlen = salen;
	return session;
}

static void rtsp_transport_tcp_ondisconnected(void* param)
{
	struct rtsp_tcp_session_t *session;
	session = (struct rtsp_tcp_session_t *)param;
    locker_lock(&session->locker);
    session->session = NULL;
    locker_unlock(&session->locker);
    rtsp_transport_tcp_session_release(session);
}

static int rtsp_transport_tcp_onrecv(void* param, const void* msg, size_t bytes)
{
	int remain;
	struct rtsp_tcp_session_t *session;
	session = (struct rtsp_tcp_session_t *)param;

	assert(bytes > 0);
	remain = (int)bytes;
	if(0 == rtsp_parser_input(session->parser, msg, &remain))
	{
        atomic_increment32(&session->ref);

		// call
		// user must reply(send/send_vec/send_file) in handle
		session->handler.onrecv(session->ptr, session, (struct sockaddr*)&session->addr, session->addrlen, session->parser, &session->data);
		return 0;
	}
	else
	{
		// wait more data
		return 1;
	}
}

static int rtsp_transport_tcp_onsend(void* param, int code, size_t bytes)
{
	struct rtsp_tcp_session_t *session;
	session = (struct rtsp_tcp_session_t *)param;
	session->handler.onsend(session->ptr, session->data, code, bytes);

	rtsp_parser_clear(session->parser);
	rtsp_transport_tcp_session_release(session);
	return 1;
}

static void* rtsp_transport_tcp_create(socket_t socket, const struct rtsp_transport_handler_t *handler, void* ptr)
{
	struct rtsp_tcp_transport_t *transport;
	struct aio_tcp_transport_handler_t aiohandler;

	transport = (struct rtsp_tcp_transport_t*)malloc(sizeof(*transport));
	if(!transport) return NULL;

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

static int rtsp_transport_tcp_send(void* param, const void* msg, size_t bytes)
{
    int r = -1;
    struct rtsp_tcp_session_t *session;
    session = (struct rtsp_tcp_session_t *)param;
    
    locker_lock(&session->locker);
	if(session->session)
    {
        r = aio_tcp_transport_send(session->session, msg, bytes);
    }
    locker_unlock(&session->locker);
    
    if(0 != r)
    {
        rtsp_transport_tcp_session_release(session);
    }

    return r;
}

static int rtsp_transport_tcp_sendv(void* session, socket_bufvec_t *vec, int n)
{
	return aio_tcp_transport_sendv(session, vec, n);
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
