#include "rtmp-server.h"
#include "rtmp-internal.h"
#include "rtmp-msgtypeid.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <assert.h>

struct rtmp_server_t
{
	struct rtmp_t rtmp;

	void* param;
	struct rtmp_server_handler_t handler;
};

static int rtmp_server_handler(void* p, struct rtmp_chunk_header_t* header, const uint8_t* payload)
{
	struct rtmp_server_t* ctx;
	ctx = (struct rtmp_server_t*)p;
	if (RTMP_TYPE_INVOKE == header->type)
	{
		return rtmp_invoke_handler(&ctx->rtmp, header, payload);
	}
	else if (RTMP_TYPE_NOTIFY == header->type || RTMP_TYPE_FLEX_STREAM == header->type)
	{
	}
	else if (RTMP_TYPE_SHARED_OBJECT == header->type || RTMP_TYPE_FLEX_OBJECT == header->type)
	{
	}
	else if (RTMP_TYPE_METADATA == header->type)
	{
	}
	else if (RTMP_TYPE_EVENT == header->type)
	{
		// User Control Message Events
		return rtmp_event_handler(&ctx->rtmp, header, payload);
	}
	else if(RTMP_TYPE_SET_CHUNK_SIZE <= header->type && RTMP_TYPE_SET_PEER_BANDWIDTH >= header->type)
	{
		// Protocol Control Messages
		// 1 ~ 6 (except 4)
		return rtmp_control_handler(&ctx->rtmp, header, payload);
	}
	else
	{
		assert(0);
		printf("%s: unknown rtmp header type: %d\n", __FUNCTION__, (int)header->type);
	}

	return 0;
}

void* rtmp_server_create(void* param, const struct rtmp_server_handler_t* handler)
{
	struct rtmp_server_t* ctx;
	ctx = (struct rtmp_server_t*)malloc(sizeof(*ctx));
	if (NULL == ctx)
		return NULL;

	memset(ctx, 0, sizeof(*ctx));
	memcpy(&ctx->handler, handler, sizeof(ctx->handler));
	ctx->rtmp.u.server.onhandler = rtmp_server_handler;
	ctx->rtmp.param = ctx;
	ctx->param = param;
	return ctx;
}

void rtmp_server_destroy(void** rtmp)
{
	if (rtmp && *rtmp)
	{
		free(*rtmp);
		*rtmp = NULL;
	}
}

int rtmp_server_state(void* p)
{
	struct rtmp_server_t* ctx;
	ctx = (struct rtmp_server_t*)p;
	return -1;
}

int rtmp_server_send(void* p)
{
	struct rtmp_server_t* ctx;
	ctx = (struct rtmp_server_t*)p;
	return -1;
}

int rtmp_server_input(void* p, const uint8_t* data, size_t bytes)
{
	struct rtmp_server_t* ctx;
	ctx = (struct rtmp_server_t*)p;
	return rtmp_chunk_input(&ctx->rtmp, data, bytes);
}
