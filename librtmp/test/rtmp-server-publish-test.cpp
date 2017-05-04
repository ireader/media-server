#include "sockutil.h"
#include "rtmp-server.h"
#include "flv-writer.h"
#include "sys/thread.h"
#include "sys/system.h"
#include <string.h>
#include <assert.h>

static void* s_flv;

static int rtmp_server_send(void* param, const void* data, size_t bytes)
{
	socket_t* c = (socket_t*)param;
	return socket_send_all_by_time(*c, data, bytes, 0, 10 * 1000);
}

static void rtmp_server_onerror(void* param, int code, const char* msg)
{
	printf("rtmp_server_onerror(%d, %s)\n", code, msg);
}

static int rtmp_server_onpublish(void* param, const char* app, const char* stream, const char* type)
{
	printf("rtmp_server_onpublish(%s, %s, %s)\n", app, stream, type);
	return 0;
}

static void rtmp_server_onvideo(void* param, const void* data, size_t bytes, uint32_t timestamp)
{
	flv_writer_input(s_flv, 9, data, bytes, timestamp);
}

static void rtmp_server_onaudio(void* param, const void* data, size_t bytes, uint32_t timestamp)
{
	flv_writer_input(s_flv, 8, data, bytes, timestamp);
}

void rtmp_server_publish_test(const char* flv)
{
	int r;
	struct rtmp_server_handler_t handler;
	memset(&handler, 0, sizeof(handler));
	handler.send = rtmp_server_send;
	handler.onerror = rtmp_server_onerror;
	//handler.oncreate_stream = rtmp_server_oncreate_stream;
	//handler.ondelete_stream = rtmp_server_ondelete_stream;
	//handler.onplay = rtmp_server_onplay;
	//handler.onpause = rtmp_server_onpause;
	//handler.onseek = rtmp_server_onseek;
	handler.onpublish = rtmp_server_onpublish;
	handler.onvideo = rtmp_server_onvideo;
	handler.onaudio = rtmp_server_onaudio;

	socket_init();
	socket_t s = socket_tcp_listen(NULL, 1935, 10);
	socket_t c = socket_accept(s, NULL, NULL);

	s_flv = flv_writer_create(flv);
	void* rtmp = rtmp_server_create(&c, &handler);

	static unsigned char packet[8 * 1024 * 1024];
	while ((r = socket_recv(c, packet, sizeof(packet), 0)) > 0)
	{
		r = rtmp_server_input(rtmp, packet, r);
	}

	rtmp_server_destroy(rtmp);
	flv_writer_destroy(s_flv);
	socket_close(c);
	socket_close(s);
	socket_cleanup();
}
