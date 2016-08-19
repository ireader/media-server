#include "cstringext.h"
#include "aio-udp-transport.h"
#include "rtsp-transport.h"
#include "rtsp-parser.h"
#include "udpsocket.h"

struct rtsp_udp_transport_t
{
	struct rtsp_transport_handler_t handler;
	void* ptr;
	void* transport;
};

static void rtsp_transport_udp_onrecv(void* ptr, void* session, const void* msg, size_t bytes, const struct sockaddr* addr, socklen_t addrlen, void** user)
{
	int remain;
	void* parser;
	struct rtsp_udp_transport_t *transport;
	transport = (struct rtsp_udp_transport_t*)ptr;

	remain = (int)bytes;
	parser = rtsp_parser_create(RTSP_PARSER_SERVER);
	if(0 == rtsp_parser_input(parser, msg, &remain))
	{
		// callback
		aio_udp_transport_addref(session);
		transport->handler.onrecv(transport->ptr, session, addr, addrlen, parser, user);
	}
	else
	{
		assert(0);
	}

	rtsp_parser_destroy(parser);
}

static void rtsp_transport_udp_onsend(void* ptr, void* session, void* user, int code, size_t bytes)
{
	struct rtsp_udp_transport_t *transport;
	transport = (struct rtsp_udp_transport_t*)ptr;
	transport->handler.onsend(transport->ptr, user, code, bytes);
	aio_udp_transport_release(session);
}

static void* rtsp_transport_udp_create(socket_t socket, const struct rtsp_transport_handler_t *handler, void* ptr)
{
	struct rtsp_udp_transport_t *transport;
	struct aio_udp_transport_handler_t aiohandler;

	transport = (struct rtsp_udp_transport_t*)malloc(sizeof(*transport));
	if(!transport) return NULL;

	memset(transport, 0, sizeof(*transport));
	memcpy(&transport->handler, handler, sizeof(transport->handler));
	transport->ptr = ptr;

	memset(&aiohandler, 0, sizeof(aiohandler));
	aiohandler.onrecv = rtsp_transport_udp_onrecv;
	aiohandler.onsend = rtsp_transport_udp_onsend;
	transport->transport = aio_udp_transport_create(socket, &aiohandler, transport);

	return transport;
}

static int rtsp_transport_udp_destroy(void* t)
{
	struct rtsp_udp_transport_t *transport;
	transport = (struct rtsp_udp_transport_t*)t;
	aio_udp_transport_destroy(transport->transport);
	free(transport);
	return 0;
}

static int rtsp_transport_udp_send(void* session, const void* msg, size_t bytes)
{
	int r;
	r = aio_udp_transport_send(session, msg, bytes);
	if(0 != r)
		aio_udp_transport_release(session);
	return r;
}

static int rtsp_transport_udp_sendv(void* session, socket_bufvec_t *vec, int n)
{
	int r;
	r = aio_udp_transport_sendv(session, vec, n);
	if(0 != r)
		aio_udp_transport_release(session);
	return r;
}

struct rtsp_transport_t* rtsp_transport_udp()
{
	static struct rtsp_transport_t transport;
	transport.create = rtsp_transport_udp_create;
	transport.destroy = rtsp_transport_udp_destroy;
	transport.send = rtsp_transport_udp_send;
	transport.sendv = rtsp_transport_udp_sendv;
	return &transport;
}
