#include "rtsp-client-transport-tcp.h"
#include "cstringext.h"
#include "sys/sock.h"
#include "rtsp-url-parser.h"

typedef struct _rtsp_client_transport_tcp_t
{
	socket_t sock; // rtsp connection
	char ip[64];
	int port;
} tcp_transport_t;

void* rtsp_client_connection_create()
{
	ctx->sock = socket_invalid;
}

void rtsp_client_connection_destroy(void* client)
{
}

static int rtsp_client_connect(struct tcp_transport_t* tcp)
{
	int r;
	if(socket_invalid == tcp->sock || 0!=socket_readable(tcp->sock))
	{
		socket_close(tcp->sock);
		tcp->sock = socket_invalid;
	}

	if(socket_invalid == ctx->sock)
	{
		ctx->sock = socket_tcp();
		r = socket_connect_ipv4_by_time(ctx->sock, ctx->ip, ctx->port, 5000);
		if(0 != r)
		{
			socket_close(ctx->sock);
			ctx->sock = socket_invalid;
			return r;
		}

		socket_setnonblock(ctx->sock, 0); // reset to block mode
	}

	return 0;
}

static int rtsp_client_disconnect(struct rtsp_context* ctx)
{
	if(socket_invalid != ctx->sock)
	{
		socket_close(ctx->sock);
		ctx->sock = socket_invalid;
	}
	return 0;
}

static int rtsp_client_send(struct rtsp_context* ctx, const void* p, int n)
{
	int r;
	r = socket_send(ctx->sock, p, n, 0);
	if(r != n)
		return -1;

	return 0;
}

static int rtsp_client_recv(struct rtsp_context* ctx)
{
	int r, s;
	char p[1024];

	rtsp_parser_clear(ctx->parser);

	s = 1;
	do
	{
		r = socket_recv(ctx->sock, p, sizeof(p), 0);
		if(r <= 0)
			break;

		s = rtsp_parser_input(ctx->parser, p, &r);
	} while(0 != s);

	if(0 == s)
	{
		onreply(rtsp, 0, ctx->parser);
		//return 200 == rtsp_get_status_code(ctx->parser) ? 0 : -1;
	}
	return -1;
}

int rtsp_client_request(void* ptr, const char* uri, const void* req, int bytes, void* rtsp, rtsp_onreply onreply)
{
	int scheme;
	int port;
	char host[64];

	if(0 != rtsp_url_parse(uri, &scheme, host, sizeof(host), &port))
		return -1;

	int r = rtsp_client_connect(ctx);
	if(0 != r)
		return r;

	r = rtsp_send(ctx, p, n);
	if(0 != r)
		return r;

	r = rtsp_recv(ctx);
	if(0 != r)
		return r;

	return 0;
}

static socket_t rtp_udp_socket(unsigned short port)
{
	socket_t s;
	s = socket_udp();
	if(socket_invalid == s)
		return s;

	if(0 == socket_bind_any(s, port))
		return s;

	socket_close(s);
	return socket_invalid;
}

struct rtsp_client_t * rtsp_client_connection_create()
{
}

void rtsp_client_connection_destroy(struct rtsp_client_t *client);
{
}
