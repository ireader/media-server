#include "rtmp-client.h"
#include "flv-demuxer.h"
#include "flv-proto.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static int rtmp_client_send(void* /*param*/, const void* /*header*/, size_t len, const void* /*data*/, size_t bytes)
{
	return len + bytes;
}

static int rtmp_client_onaudio(void* param, const void* data, size_t bytes, uint32_t timestamp)
{
	flv_demuxer_t* flv = (flv_demuxer_t*)param;
	printf("[A] timestamp: %u, bytes: %u\n", timestamp, (unsigned int)bytes);
	return flv_demuxer_input(flv, FLV_TYPE_AUDIO, data, bytes, timestamp);
}

static int rtmp_client_onvideo(void* param, const void* data, size_t bytes, uint32_t timestamp)
{
	flv_demuxer_t* flv = (flv_demuxer_t*)param;
	printf("[V] timestamp: %u, bytes: %u\n", timestamp, (unsigned int)bytes);
	return flv_demuxer_input(flv, FLV_TYPE_VIDEO, data, bytes, timestamp);
}

static int rtmp_client_onscript(void* /*param*/, const void* data, size_t bytes, uint32_t timestamp)
{
	printf("[S] timestamp: %u, bytes: %u\n", timestamp, (unsigned int)bytes);
	return 0;
}

static int OnFLV(void* param, int codec, const void* data, size_t bytes, uint32_t pts, uint32_t dts, int flags)
{
	printf("[FLV] codec: %04x, pts: %u, dts: %u, byte: %u\n", codec, (unsigned int)bytes, (unsigned int)pts, (unsigned int)dts);
	return 0;
}

void rtmp_input_test(const char* file)
{
	static char packet[1024];

	flv_demuxer_t* flv = flv_demuxer_create(OnFLV, NULL);

	struct rtmp_client_handler_t handler;
	memset(&handler, 0, sizeof(handler));
	handler.send = rtmp_client_send;
	handler.onaudio = rtmp_client_onaudio;
	handler.onvideo = rtmp_client_onvideo;
	handler.onscript = rtmp_client_onscript;
	rtmp_client_t* rtmp = rtmp_client_create("1", "1", "1", flv, &handler);
	
	int r = rtmp_client_start(rtmp, 1);

	int n = 0;
	FILE* fp = fopen(file, "rb");
	while (fp && (n = fread(packet, 1, sizeof(packet), fp)) > 0)
	{
		assert(0 == rtmp_client_input(rtmp, packet, n));
	}
	fclose(fp);

	flv_demuxer_destroy(flv);
	rtmp_client_stop(rtmp);
	rtmp_client_destroy(rtmp);
}