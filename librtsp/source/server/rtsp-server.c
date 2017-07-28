#include "rtsp-server.h"
#include "rtsp-reason.h"
#include "rfc822-datetime.h"
#include "rtsp-server-internal.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static void rtsp_server_onsend(struct rtsp_session_t* session, int code, size_t bytes)
{
	(void)session;
	(void)code;
	(void)bytes;
}

struct rtsp_server_t* rtsp_server_create(const char* ip, int port, struct rtsp_handler_t* handler, void* param)
{
	struct rtsp_server_t* server;

	server = (struct rtsp_server_t *)calloc(1, sizeof(struct rtsp_server_t));
	if(!server) return NULL;

	server->onrecv = rtsp_server_handle;
	server->onsend = rtsp_server_onsend;
	server->param = param;
	memcpy(&server->handler, handler, sizeof(server->handler));

	server->tcp = rtsp_server_listen(ip, port, server);
	server->udp = rtsp_transport_udp_create(ip, port, server);
	if(!server->udp || !server->tcp)
	{
		rtsp_server_destroy(server);
		return NULL;
	}

	return server;
}

int rtsp_server_destroy(struct rtsp_server_t* ctx)
{
	if (ctx->tcp)
	{
		rtsp_server_unlisten(ctx->tcp);
		ctx->tcp = NULL;
	}

	if (ctx->udp)
	{
		rtsp_transport_udp_destroy(ctx->udp);
		ctx->udp = NULL;
	}

	free(ctx); // TODO: error
	return 0;
}

const char* rtsp_server_get_header(struct rtsp_session_t *session, const char* name)
{
	return rtsp_get_header_by_name(session->parser, name);
}

int rtsp_server_get_client(struct rtsp_session_t *session, char ip[65], unsigned short *port)
{
	if (NULL == ip || NULL == port)
		return -1;
	return socket_addr_to((struct sockaddr*)&session->addr, session->addrlen, ip, port);
}

int rtsp_server_reply(struct rtsp_session_t *session, int code)
{
	int len;
	rfc822_datetime_t datetime;
	rfc822_datetime_format(time(NULL), datetime);

	len = snprintf(session->reply, sizeof(session->reply),
		"RTSP/1.0 %d %s\r\n"
		"CSeq: %u\r\n"
		"Date: %s\r\n"
		"\r\n",
		code, rtsp_reason_phrase(code), session->cseq, datetime);

	return session->send(session, session->reply, len);
}
