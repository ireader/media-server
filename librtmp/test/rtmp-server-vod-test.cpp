#include "sockutil.h"
#include "rtmp-server.h"
#include "flv-reader.h"
#include "sys/thread.h"
#include "sys/system.h"
#include <string.h>
#include <assert.h>

static pthread_t t;
static void* s_rtmp;
static const char* s_file;

static int STDCALL rtmp_server_worker(void* param)
{
	int r, type;
	uint32_t timestamp;
	static uint32_t s_timestamp = 0;
	void* f = flv_reader_create(s_file);

	static unsigned char packet[2 * 1024 * 1024];
	while ((r = flv_reader_read(f, &type, &timestamp, packet, sizeof(packet))) > 0)
	{
		if (timestamp > s_timestamp)
			system_sleep(timestamp - s_timestamp);
		s_timestamp = timestamp;

		if (8 == type)
		{
			rtmp_server_send_audio(s_rtmp, packet, r, timestamp);
		}
		else if (9 == type)
		{
			rtmp_server_send_video(s_rtmp, packet, r, timestamp);
		}
		else
		{
			assert(0);
		}
	}

	flv_reader_destroy(f);
	thread_destroy(t);
	return 0;
}

static int rtmp_server_send(void* param, const void* header, size_t len, const void* data, size_t bytes)
{
	socket_t* socket = (socket_t*)param;
	if (bytes > 0 && data)
	{
		socket_bufvec_t vec[2];
		socket_setbufvec(vec, 0, (void*)header, len);
		socket_setbufvec(vec, 1, (void*)data, bytes);
		return socket_send_v_all_by_time(*socket, vec, 2, 0, 2000);
	}
	else
	{
		return socket_send_all_by_time(*socket, data, bytes, 0, 2000);
	}
}

static int rtmp_server_onplay(void* param, const char* app, const char* stream, double start, double duration, uint8_t reset)
{
	printf("rtmp_server_onplay(%s, %s, %f, %f, %d)\n", app, stream, start, duration, (int)reset);

	return thread_create(&t, rtmp_server_worker, param);
}

static int rtmp_server_onpause(void* param, int pause, uint32_t ms)
{
	printf("rtmp_server_onpause(%d, %u)\n", pause, (unsigned int)ms);
	return 0;
}

static int rtmp_server_onseek(void* param, uint32_t ms)
{
	printf("rtmp_server_onseek(%u)\n", (unsigned int)ms);
	return 0;
}

void rtmp_server_vod_test(const char* flv)
{
	int r;
	struct rtmp_server_handler_t handler;
	memset(&handler, 0, sizeof(handler));
	handler.send = rtmp_server_send;
	//handler.oncreate_stream = rtmp_server_oncreate_stream;
	//handler.ondelete_stream = rtmp_server_ondelete_stream;
	handler.onplay = rtmp_server_onplay;
	handler.onpause = rtmp_server_onpause;
	handler.onseek = rtmp_server_onseek;
	//handler.onpublish = rtmp_server_onpublish;
	//handler.onvideo = rtmp_server_onvideo;
	//handler.onaudio = rtmp_server_onaudio;

	socket_init();

	socklen_t n;
	struct sockaddr_storage ss;
	socket_t s = socket_tcp_listen(NULL, 1935, 10);
	socket_t c = socket_accept(s, &ss, &n);

	s_file = flv;
	s_rtmp = rtmp_server_create(&c, &handler);

	static unsigned char packet[8 * 1024 * 1024];
	while ((r = socket_recv(c, packet, sizeof(packet), 0)) > 0)
	{
		r = rtmp_server_input(s_rtmp, packet, r);
	}
	
	rtmp_server_destroy(s_rtmp);
	socket_close(c);
	socket_close(s);
	socket_cleanup();
}
