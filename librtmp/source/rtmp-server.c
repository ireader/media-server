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
	struct rtmp_t ctx;

	void* param;
	struct rtmp_server_handler_t handler;
};

static int rtmp_server_handler(void* p, struct rtmp_chunk_header_t* header, const uint8_t* payload)
{
	struct rtmp_server_t* rtmp;
	rtmp = (struct rtmp_server_t*)p;
	if (RTMP_TYPE_INVOKE == header->type)
	{
		rtmp_invoke_handler();
	}
	else if (RTMP_TYPE_NOTIFY == header->type)
	{
	}
	return -1;
}

void* rtmp_server_create(void* param, const struct rtmp_server_handler_t* handler)
{
	struct rtmp_server_t* rtmp;
	rtmp = (struct rtmp_server_t*)malloc(sizeof(*rtmp));
	if (NULL == rtmp)
		return NULL;

	memset(rtmp, 0, sizeof(*rtmp));
	memcpy(&rtmp->handler, handler, sizeof(rtmp->handler));
	rtmp->ctx.onpacket = rtmp_server_handler;
	rtmp->ctx.param = rtmp;
	rtmp->param = param;
	return rtmp;
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
	struct rtmp_server_t* rtmp;
	rtmp = (struct rtmp_server_t*)p;
	return -1;
}

int rtmp_server_send(void* p)
{
	struct rtmp_server_t* rtmp;
	rtmp = (struct rtmp_server_t*)p;
	return -1;
}

int rtmp_server_input(void* p, const uint8_t* data, size_t bytes)
{
	struct rtmp_server_t* rtmp;
	rtmp = (struct rtmp_server_t*)p;
	return rtmp_chunk_input(&rtmp->ctx, data, bytes);
}
