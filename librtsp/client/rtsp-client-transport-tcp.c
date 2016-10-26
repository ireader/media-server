#include "rtsp-client-transport-tcp.h"
#include "cstringext.h"
#include "sys/sock.h"
#include "rtsp-url-parser.h"
#include "rtsp-parser.h"

#define N_TRANSPORT 3
#define N_RTP_PACKAGE 1024

struct rtsp_tcp_transport_t
{
	socket_t sock; // rtsp connection
	char ip[SOCKET_ADDRLEN]; // rtsp server
	int port;

	void* parser; // rtsp parser
};

struct rtsp_transport_context_t
{
	struct rtsp_tcp_transport_t transport[N_TRANSPORT];
	int count;

	void* ptr;
	rtsp_ondata ondata;
};

void* rtsp_client_tcp_transport_create(rtsp_ondata ondata, void* ptr)
{
	struct rtsp_transport_context_t *transport;
	transport = (struct rtsp_transport_context_t *)malloc(sizeof(*transport));
	if(transport)
	{
		memset(transport, 0, sizeof(*transport));
		transport->count = 0;
		transport->ondata = ondata;
		transport->ptr = ptr;
	}
	return transport;
}

int rtsp_client_tcp_transport_destroy(void* t)
{
	int i;
	struct rtsp_transport_context_t *transport;
	transport = (struct rtsp_transport_context_t *)t;

	for(i = 0; i < transport->count; i++)
	{
		assert(0 != transport->transport[i].sock);
		if(socket_invalid != transport->transport[i].sock)
			socket_close(transport->transport[i].sock);
	}

#if defined(_DEBUG) || defined(DEBUG)
	memset(transport, 0, sizeof(*transport));
#endif
	free(transport);
	return 0;
}

static int rtsp_client_connect(const char* ip, int port, socket_t* socket)
{
	if(socket_invalid == *socket || 0!=socket_readable(*socket))
	{
		socket_close(*socket);
		*socket = socket_invalid;
	}

	if(socket_invalid == *socket)
	{
		*socket = socket_connect_host(ip, (u_short)port, 5000);
		if(socket_invalid == *socket)
			return socket_geterror();

		socket_setnonblock(*socket, 0); // reset to block mode
	}

	return 0;
}

static int rtsp_client_disconnect(socket_t* socket)
{
	if(socket_invalid != *socket)
	{
		socket_close(*socket);
		*socket = socket_invalid;
	}
	return 0;
}

static int rtsp_client_send(socket_t socket, const void* p, size_t n)
{
	int r;
	r = socket_send(socket, p, n, 0);
	if((size_t)r != n)
		return -1;

	return 0;
}

static int rtsp_client_recv(struct rtsp_tcp_transport_t* transport, void* rtsp, rtsp_onreply onreply, rtsp_ondata ondata, void* ptr)
{
	int r, s;
	unsigned char p[N_RTP_PACKAGE+3];

	rtsp_parser_clear(transport->parser);

	s = 1;
	r = socket_recv(transport->sock, p, sizeof(p), 0);
	if('$' == *p)
	{
		// RTP over RTSP
		// RFC2326 10.12 Embedded (Interleaved) Binary Data (p40)
		int channel; // 8bits
		unsigned int len; // 16bits

		if(r < 3)
		{
			s = socket_recv_all_by_time(transport->sock, p+r, 3-r, 0, 5000);
			if(s != 3-r)
				return -1; // recv error
			
			r = 3;
		}

		channel = p[1];
		len = (p[2] << 8) | p[3];

		assert(len+3 < sizeof(p));
		if((size_t)r < len + 3 && len+3 < sizeof(p))
		{
			s = socket_recv_all_by_time(transport->sock, p+r, len+3-r, 0, 10000);
			if((size_t)s != len+3-(size_t)r)
				return -1; // recv error

			r += s;
		}

		assert(r < sizeof(p));
		ondata(ptr, channel, p+3, r-3);
	}
	else
	{
		while(r > 0)
		{
			s = rtsp_parser_input(transport->parser, p, &r);
			if(0 == s)
			{
				onreply(rtsp, 0, transport->parser);
				break;
			}

			r = socket_recv(transport->sock, p, sizeof(p), 0);
		}

		if(0 != s)
			onreply(rtsp, -1, NULL);
	}

	return 0==s ? 0 : -1;
}

static struct rtsp_tcp_transport_t* rtsp_client_transport_find(struct rtsp_transport_context_t *ctx, const char* ip, int port)
{
	int i;
	for(i = 0; i < ctx->count; i++)
	{
		struct rtsp_tcp_transport_t *transport;
		transport = ctx->transport + i; 
		if(transport->port == port && 0 == strcasecmp(transport->ip, ip))
			return transport;
	}
	return NULL;
}

int rtsp_client_tcp_transport_request(void* t, const char* uri, const void* req, size_t bytes, void* rtsp, rtsp_onreply onreply)
{
	int scheme;
	int port;
	char ip[SOCKET_ADDRLEN];
	int r;

	struct rtsp_transport_context_t *ctx;
	struct rtsp_tcp_transport_t *transport;
	ctx = (struct rtsp_transport_context_t *)t;

	if(0 != rtsp_url_parse(uri, &scheme, ip, sizeof(ip), &port))
		return -1;

	transport = rtsp_client_transport_find(ctx, ip, port);
	if(!transport)
	{
		if(ctx->count + 1 > sizeof(ctx->transport)/sizeof(ctx->transport[0]))
			return -1;

		transport = &ctx->transport[ctx->count++];
		transport->sock = socket_invalid;
		strlcpy(transport->ip, ip, sizeof(transport->ip));
		transport->port = port;
		transport->parser = rtsp_parser_create(RTSP_PARSER_CLIENT);
	}

	r = rtsp_client_connect(transport->ip, transport->port, &transport->sock);
	if(0 != r)
		return r;

	r = rtsp_client_send(transport->sock, req, bytes);
	if(0 != r)
		return r;

	return rtsp_client_recv(transport, rtsp, onreply, ctx->ondata, ctx->ptr);
	//return 0;
}
