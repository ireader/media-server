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

static void* rtsp_transport_udp_onrecv(void* ptr, void* session, const void* msg, size_t bytes, const char* ip, int port)
{
	int remain;
	void* parser;
	void* user = NULL;
	struct rtsp_udp_transport_t *transport;
	transport = (struct rtsp_udp_transport_t*)ptr;

	remain = bytes;
	parser = rtsp_parser_create(RTSP_PARSER_SERVER);
	if(0 == rtsp_parser_input(parser, msg, &remain))
	{
		// callback
		user = transport->handler.onrecv(transport->ptr, session, ip, port, parser);
	}
	else
	{
		assert(0);
	}

	rtsp_parser_destroy(parser);
	return user;
}

static void rtsp_transport_udp_onsend(void* ptr, void* session, int code, size_t bytes)
{
	struct rtsp_udp_transport_t *transport;
	transport = (struct rtsp_udp_transport_t*)ptr;
	transport->handler.onsend(transport->ptr, session, code, bytes);
}

static void* rtsp_transport_udp_create(socket_t socket, const struct rtsp_transport_handler_t *handler, void* ptr)
{
	struct rtsp_udp_transport_t *transport;
	struct aio_udp_transport_handler_t aiohandler;

	transport = (struct rtsp_udp_transport_t*)malloc(sizeof(*transport));
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
	return aio_udp_transport_send(session, msg, bytes);
}

static int rtsp_transport_udp_sendv(void* session, socket_bufvec_t *vec, int n)
{
	return aio_udp_transport_sendv(session, vec, n);
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
