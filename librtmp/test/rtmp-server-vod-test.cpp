#include "sockutil.h"
#include "rtmp-server.h"
#include "flv-reader.h"
#include "flv-proto.h"
#include "sys/thread.h"
#include "sys/system.h"
#include <string.h>
#include <assert.h>

static pthread_t t;
static rtmp_server_t* s_rtmp;
static const char* s_file;

static int STDCALL rtmp_server_worker(void* param)
{
	int r, type;
	uint32_t timestamp;
	static uint64_t clock0 = system_clock() - 200; // send more data, open fast
	void* f = flv_reader_create(s_file);

	static unsigned char packet[8 * 1024 * 1024];
	while ((r = flv_reader_read(f, &type, &timestamp, packet, sizeof(packet))) > 0)
	{
		assert(r < sizeof(packet));
		uint64_t t = system_clock();
		if (clock0 + timestamp > t && clock0 + timestamp < t + 3 * 1000)
			system_sleep(clock0 + timestamp - t);
		else if (clock0 + timestamp > t + 3 * 1000)
			clock0 = t - timestamp;

		if (FLV_TYPE_AUDIO == type)
		{
			r = rtmp_server_send_audio(s_rtmp, packet, r, timestamp);
		}
		else if (FLV_TYPE_VIDEO == type)
		{
			r = rtmp_server_send_video(s_rtmp, packet, r, timestamp);
		}
		else if (FLV_TYPE_SCRIPT == type)
		{
			r = rtmp_server_send_script(s_rtmp, packet, r, timestamp);
		}
		else
		{
			assert(0);
			r = 0;
		}

		if (0 != r)
		{
			assert(0);
			break; // TODO: handle send failed
		}
	}

	flv_reader_destroy(f);
	thread_destroy(t);
	return 0;
}

static int rtmp_server_send(void* param, const void* header, size_t len, const void* data, size_t bytes)
{
	socket_t* socket = (socket_t*)param;
	socket_bufvec_t vec[2];
	socket_setbufvec(vec, 0, (void*)header, len);
	socket_setbufvec(vec, 1, (void*)data, bytes);
	return socket_send_v_all_by_time(*socket, vec, bytes > 0 ? 2 : 1, 0, 20000);
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

static int rtmp_server_ongetduration(void* param, const char* app, const char* stream, double* duration)
{
	*duration = 30 * 60;
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
	handler.ongetduration = rtmp_server_ongetduration;

	socket_init();

	socklen_t n;
	struct sockaddr_storage ss;
	socket_t s = socket_tcp_listen(NULL, 1935, SOMAXCONN);
	socket_t c = socket_accept(s, &ss, &n);

	s_file = flv;
	s_rtmp = rtmp_server_create(&c, &handler);

	static unsigned char packet[2 * 1024 * 1024];
	while ((r = socket_recv(c, packet, sizeof(packet), 0)) > 0)
	{
		assert(0 == rtmp_server_input(s_rtmp, packet, r));
	}
	
	rtmp_server_destroy(s_rtmp);
	socket_close(c);
	socket_close(s);
	socket_cleanup();
}
