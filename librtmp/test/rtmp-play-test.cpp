#include "sockutil.h"
#include "rtmp-client.h"
#include "flv-writer.h"
#include <assert.h>
#include <stdio.h>

static void* s_flv;

static int rtmp_client_send(void* param, const void* data, size_t bytes)
{
	socket_t* socket = (socket_t*)param;
	return socket_send(*socket, data, bytes, 0);
}

static void rtmp_client_onerror(void* /*param*/, int code, const char* msg)
{
	printf("rtmp_onerror code: %d, msg: %s\n", code, msg);
}

static void rtmp_client_onaudio(void* /*param*/, const void* data, size_t bytes, uint32_t timestamp)
{
	flv_writer_input(s_flv, 8, data, bytes, timestamp);
}

static void rtmp_client_onvideo(void* /*param*/, const void* data, size_t bytes, uint32_t timestamp)
{
	flv_writer_input(s_flv, 9, data, bytes, timestamp);
}

static void rtmp_client_onmeta(void* /*param*/, const void* /*data*/, size_t /*bytes*/)
{
}

// rtmp_play_test("rtmp://strtmpplay.cdn.suicam.com/carousel/51632");
void rtmp_play_test(const char* host, const char* app, const char* stream, const char* flv)
{
	static char packet[8 * 1024 * 1024];
	snprintf(packet, sizeof(packet), "rtmp://%s/%s/%s", host, app, stream); // tcurl

	socket_init();
	socket_t socket = socket_connect_host(host, 1935, 2000);
	socket_setnonblock(socket, 0);

	struct rtmp_client_handler_t handler;
	handler.send = rtmp_client_send;
	handler.onerror = rtmp_client_onerror;
	handler.onmeta = rtmp_client_onmeta;
	handler.onaudio = rtmp_client_onaudio;
	handler.onvideo = rtmp_client_onvideo;
	void* rtmp = rtmp_client_create(app, stream, packet/*tcurl*/, &socket, &handler);
	s_flv = flv_writer_create(flv);

	int r = rtmp_client_start(rtmp, 1);

	while ((r = socket_recv(socket, packet, sizeof(packet), 0)) > 0)
	{
		r = rtmp_client_input(rtmp, packet, r);
	}

	rtmp_client_stop(rtmp);
	flv_writer_destroy(s_flv);
	rtmp_client_destroy(rtmp);
	socket_close(socket);
	socket_cleanup();
}
