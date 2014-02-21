#include "rtsp-server.h"
#include "cstringext.h"
#include "sys/sock.h"
#include "sys/sync.h"
#include "sys/system.h"
#include "aio-socket.h"
#include "time64.h"
#include "tcpserver.h"

struct rtsp_context
{
	char *ip;
	int port;

	locker_t locker;
};

void* rtsp_server_start(const char* ip, int port)
{
	struct rtsp_context* ctx;
	ctx = (struct rtsp_context *)malloc(sizeof(struct rtsp_context));
	if(!ctx)
		return NULL;

	memset(ctx, 0, sizeof(struct rtsp_context));
	ctx->ip = ip ? strdup(ip) : NULL;
	ctx->port = port;
	locker_create(&ctx->locker);
	return ctx;
}

int rtsp_server_stop(void* server)
{
	struct rtsp_context* ctx;
	ctx = (struct rtsp_context*)server;
	locker_destroy(&ctx->locker);
	return 0;
}

int rtsp_server_report(void* server)
{
}
