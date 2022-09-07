#include "rtmp-server.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "flv-proto.h"
#include "flv-writer.h"

static int rtmp_server_send(void* /*param*/, const void* /*header*/, size_t len, const void* /*data*/, size_t bytes)
{
	return len + bytes;
}

static int rtmp_server_onpublish(void* param, const char* app, const char* stream, const char* type)
{
	printf("rtmp_server_onpublish(%s, %s, %s)\n", app, stream, type);
	return 0;
}

static int rtmp_server_onscript(void* flv, const void* script, size_t bytes, uint32_t timestamp)
{
	printf("[S] timestamp: %u\n", timestamp);
	return flv_writer_input(flv, FLV_TYPE_SCRIPT, script, bytes, timestamp);
}

static int rtmp_server_onvideo(void* flv, const void* data, size_t bytes, uint32_t timestamp)
{
	printf("[V] timestamp: %u\n", timestamp);
	return flv_writer_input(flv, FLV_TYPE_VIDEO, data, bytes, timestamp);
}

static int rtmp_server_onaudio(void* flv, const void* data, size_t bytes, uint32_t timestamp)
{
	printf("[A] timestamp: %u\n", timestamp);
	return flv_writer_input(flv, FLV_TYPE_AUDIO, data, bytes, timestamp);
}

void rtmp_server_input_test(const char* file)
{
	static uint8_t packet[1024];

	snprintf((char*)packet, sizeof(packet), "%s.flv", file);
	void* flv = flv_writer_create((const char*)packet);

	struct rtmp_server_handler_t handler;
	memset(&handler, 0, sizeof(handler));
	handler.send = rtmp_server_send;
	handler.onpublish = rtmp_server_onpublish;
	handler.onaudio = rtmp_server_onaudio;
	handler.onvideo = rtmp_server_onvideo;
	handler.onscript = rtmp_server_onscript;
	rtmp_server_t* rtmp = rtmp_server_create(flv, &handler);

	int n = 0;
	FILE* fp = fopen(file, "rb");
	while (fp && (n = fread(packet, 1, sizeof(packet), fp)) > 0)
	{
		assert(0 == rtmp_server_input(rtmp, packet, n));
	}
	fclose(fp);

	flv_writer_destroy(flv);
	rtmp_server_destroy(rtmp);
}