#include "rtmp-server.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static int rtmp_server_send(void* /*param*/, const void* /*header*/, size_t len, const void* /*data*/, size_t bytes)
{
	return len + bytes;
}

static int rtmp_server_onpublish(void* param, const char* app, const char* stream, const char* type)
{
	printf("rtmp_server_onpublish(%s, %s, %s)\n", app, stream, type);
	return 0;
}

static int rtmp_server_onscript(void* param, const void* script, size_t bytes, uint32_t timestamp)
{
	printf("[S] timestamp: %u\n", timestamp);
	return 0;
}

static int rtmp_server_onvideo(void* param, const void* data, size_t bytes, uint32_t timestamp)
{
	printf("[V] timestamp: %u\n", timestamp);
	return 0;
}

static int rtmp_server_onaudio(void* param, const void* data, size_t bytes, uint32_t timestamp)
{
	printf("[A] timestamp: %u\n", timestamp);
	return 0;
}

void rtmp_server_input_test(const char* file)
{
	static uint8_t packet[8 * 1024 * 1024];

	struct rtmp_server_handler_t handler;
	memset(&handler, 0, sizeof(handler));
	handler.send = rtmp_server_send;
	handler.onpublish = rtmp_server_onpublish;
	handler.onaudio = rtmp_server_onaudio;
	handler.onvideo = rtmp_server_onvideo;
	handler.onscript = rtmp_server_onscript;
	rtmp_server_t* rtmp = rtmp_server_create(NULL, &handler);

	int n = 0;
	FILE* fp = fopen(file, "rb");
	while (fp && (n = fread(packet, 1, sizeof(packet), fp)) > 0)
	{
		assert(0 == rtmp_server_input(rtmp, packet, n));
	}
	fclose(fp);

	rtmp_server_destroy(rtmp);
}