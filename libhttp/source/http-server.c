#include <stdlib.h>
#include "http-server.h"
#include "cstringext.h"
#include "sys/sock.h"
#include "sys/sync.h"
#include "sys/system.h"
#include "sys/process.h"
#include "thread-pool.h"
#include "tcpserver.h"
#include "http-server-internal.h"

static int s_running;
static thread_pool_t s_pool;

static void http_server_onaccept(void* param, int code, socket_t socket, const char* ip, int port)
{
	struct http_server_t *ctx;
	struct http_session_t *session;
	ctx = (struct http_server_t*)param;

	if(0 == code)
	{
		session = http_session_run(ctx, socket, ip, port);

		// link session

		// do again
		code = aio_socket_accept(ctx->socket, http_server_onaccept, ctx);
	}
	
	if(0 != code)
	{
		assert(0);
		printf("http_server_onaccept => %d\n", code);
		//http_server_destroy(ctx);
	}
}

static void http_server_process(void* param)
{
	int r;
	do
	{
		r = aio_socket_process(2*60*1000);
		if(0 != r)
		{
			printf("http_server_process =>%d\n", r);
		}
	} while(*(int*)param && -1 != r);
}

int http_server_init()
{
	int cpu = (int)system_getcpucount();
	s_pool = thread_pool_create(cpu, 1, 64);
	aio_socket_init(cpu);

	s_running = 1;
	while(cpu-- > 0)
	{
		thread_pool_push(s_pool, http_server_process, &s_running); // start worker
	}

	return 0;
}

int http_server_cleanup()
{
	s_running = 0;
	thread_pool_destroy(s_pool);
	aio_socket_clean();
	return 0;
}

void* http_server_create(const char* ip, int port)
{
	socket_t socket;
	struct http_server_t *ctx;

	// create server socket
	socket = tcpserver_create(ip, port, 64);
	if(0 == socket)
	{
		printf("http_server_create(%s, %d): create socket error.\n", ip, port);
		return NULL;
	}

	ctx = (struct http_server_t*)malloc(sizeof(ctx[0]));
	if(!ctx)
		return NULL;

	memset(ctx, 0, sizeof(ctx[0]));
	ctx->socket = aio_socket_create(socket, 1);
	if(!ctx->socket)
	{
		printf("http_server_create(%s, %d) create aio socket error.\n", ip, port);
		http_server_destroy(ctx);
		return NULL;
	}

	if(0 != aio_socket_accept(ctx->socket, http_server_onaccept, ctx))
	{
		printf("http_server_create(%s, %d) aio accept error.\n", ip, port);
		http_server_destroy(ctx);
		return NULL;
	}

	return ctx;
}

int http_server_destroy(void* http)
{
	struct http_server_t *ctx;
	ctx = (struct http_server_t*)http;

	if(ctx->socket)
		aio_socket_destroy(ctx->socket);

	//for(ctx->head.next != &ctx->head)
	//{
	//	session = ctx->head.next;
	//	ctx->head.next = session->next;
	//	free(session);
	//}

	free(ctx);
	return 0;
}

int http_server_set_handler(void* http, http_server_handler handler, void* param)
{
	struct http_server_t *ctx;
	ctx = (struct http_server_t*)http;
	ctx->handle = handler;
	ctx->param = param;
	return 0;
}
