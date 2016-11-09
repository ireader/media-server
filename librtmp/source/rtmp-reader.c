#include "rtmp-reader.h"
#include "cstringext.h"
#include "librtmp/log.h"
#include "librtmp/rtmp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <assert.h>

#define N_URL		2048

typedef struct _RTMPContext
{
	RTMP* rtmp;
	char url[N_URL];
} RTMPContext;

void* rtmp_reader_create(const char* url)
{
	int r;
	RTMPContext* ctx;

	ctx = (RTMPContext*)malloc(sizeof(RTMPContext));
	if (!ctx) return NULL;
	memset(ctx, 0, sizeof(RTMPContext));
	strlcpy(ctx->url, url, sizeof(ctx->url));

#if defined(_DEBUG) || defined(DEBUG)
	RTMP_LogSetLevel(RTMP_LOGINFO);
#endif
	ctx->rtmp = RTMP_Alloc();
	if (!ctx->rtmp)
	{
		free(ctx);
		return NULL;
	}

	RTMP_Init(ctx->rtmp);

	r = RTMP_SetupURL(ctx->rtmp, ctx->url);
	if (1 != r)
	{
		RTMP_Free(ctx->rtmp);
		free(ctx);
		return NULL;
	}

	//ctx->rtmp->Link.timeout = 25; // in seconds
	ctx->rtmp->Link.lFlags |= RTMP_LF_LIVE;
	//RTMP_SetBufferMS(ctx->rtmp, 3600 * 1000);
	return ctx;
}

void rtmp_reader_destroy(void* p)
{
	RTMPContext* ctx;
	ctx = (RTMPContext*)p;

	if (ctx->rtmp)
	{
		RTMP_Close(ctx->rtmp);
		RTMP_Free(ctx->rtmp);
	}

	free(ctx);
}

void rtmp_reader_settimeout(void* p, int timeout)
{
	RTMPContext* ctx;
	ctx = (RTMPContext*)p;
	ctx->rtmp->Link.timeout = timeout; // in seconds
}

void rtmp_reader_resume(void* p)
{
	RTMPContext* ctx;
	ctx = (RTMPContext*)p;
	ctx->rtmp->m_read.status = 0;
}

int rtmp_reader_read(void* p, void* packet, int bytes)
{
	RTMPContext* ctx;
	ctx = (RTMPContext*)p;

	if (!RTMP_IsConnected(ctx->rtmp))
	{
		if (!RTMP_Connect(ctx->rtmp, NULL))
			return -1;

		if (!RTMP_ConnectStream(ctx->rtmp, 0))
			return -1;
	}

	return RTMP_Read(ctx->rtmp, packet, bytes);
}
