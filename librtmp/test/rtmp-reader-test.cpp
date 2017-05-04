#include "sockutil.h"
#include "rtmp-client.h"
#include "flv-writer.h"
#include <assert.h>
#include <stdio.h>

static void* flv;

static int rtmp_send(void* param, const void* data, size_t bytes)
{
	socket_t* socket = (socket_t*)param;
	return socket_send(*socket, data, bytes, 0);
}

static void rtmp_onerror(void* /*param*/, int code, const char* msg)
{
	printf("rtmp_onerror code: %d, msg: %s\n", code, msg);
}

static void rtmp_onaudio(void* /*param*/, const void* data, size_t bytes, uint32_t timestamp)
{
	flv_writer_input(flv, 8, data, bytes, timestamp);
}

static void rtmp_onvideo(void* /*param*/, const void* data, size_t bytes, uint32_t timestamp)
{
	flv_writer_input(flv, 9, data, bytes, timestamp);
}

static void rtmp_onmeta(void* param, const void* /*data*/, size_t /*bytes*/)
{
}

// rtmp_reader_test("rtmp://strtmpplay.cdn.suicam.com/carousel/51632");
void rtmp_reader_test(const char* url)
{
	socket_init();
	socket_t socket = socket_connect_host("192.168.31.129", 1935, 2000);
	socket_setnonblock(socket, 0);

	struct rtmp_client_handler_t handler;
	handler.send = rtmp_send;
	handler.onerror = rtmp_onerror;
	handler.onmeta = rtmp_onmeta;
	handler.onaudio = rtmp_onaudio;
	handler.onvideo = rtmp_onvideo;
	void* rtmp = rtmp_client_create("vod", "1.flv", "rtmp://192.168.31.129:1935/vod", &socket, &handler);
	flv = flv_writer_create("1.flv");

	int r = rtmp_client_start(rtmp, 1);

	static unsigned char packet[8 * 1024 * 1024];
	while ((r = socket_recv(socket, packet, sizeof(packet), 0)) > 0)
	{
		r = rtmp_client_input(rtmp, packet, r);
	}

	rtmp_client_stop(rtmp);
	flv_writer_destroy(flv);
	rtmp_client_destroy(rtmp);
	socket_close(socket);
	socket_cleanup();
}
