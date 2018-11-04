#include "sys/sock.h"
#include "sys/thread.h"
#include "sys/system.h"
#include "sys/sync.hpp"
#include "aio-connect.h"
#include "aio-timeout.h"
#include "aio-rtmp-client.h"
#include "flv-reader.h"
#include "flv-proto.h"
#include <stdio.h>
#include <assert.h>
#include <list>

static struct
{
	int code; // rtmp error code
	const char* file;
	char tcurl[4 * 1024];
	const char* app;
	const char* stream;
	aio_rtmp_client_t* rtmp;
} s_param;

static int STDCALL rtmp_client_push(void* flv)
{
	int r, type;
	uint32_t timestamp;
	uint64_t clock0 = system_clock();
	void* f = flv_reader_create((const char*)flv);

	static char packet[2 * 1024 * 1024];
	while (0 == s_param.code && (r = flv_reader_read(f, &type, &timestamp, packet, sizeof(packet))) > 0)
	{
		uint64_t t = system_clock();
		if(clock0 + timestamp > t && clock0 + timestamp < t + 3 * 1000)
			system_sleep(clock0 + timestamp - t);
		else if (t + timestamp > t + 3 * 1000)
			t = t - timestamp;

		while (s_param.rtmp && aio_rtmp_client_get_unsend(s_param.rtmp) > 8 * 1024 * 1024)
			system_sleep(1000); // can't send?

		switch (type)
		{
		case FLV_TYPE_AUDIO:
			r = aio_rtmp_client_send_audio(s_param.rtmp, packet, r, timestamp);
			break;

		case FLV_TYPE_VIDEO:
			r = aio_rtmp_client_send_video(s_param.rtmp, packet, r, timestamp);
			break;

		case FLV_TYPE_SCRIPT:
			r = aio_rtmp_client_send_script(s_param.rtmp, packet, r, timestamp);
			break;

		default:
			r = 0;
			break;
		}

		if (0 != r)
		{
			assert(0);
			break; // TODO: handle send failed
		}
	}

	aio_rtmp_client_destroy(s_param.rtmp);
	flv_reader_destroy(f);
	s_param.file = NULL;
	s_param.rtmp = NULL;
	return 0;
}

static void rtmp_client_publish_onsend(void*, size_t)
{
}

static void rtmp_client_publish_onready(void*)
{
	pthread_t thread;
	thread_create(&thread, rtmp_client_push, (void*)s_param.file);
	thread_detach(thread);
}

static void rtmp_client_publish_onerror(void* /*param*/, int code)
{
	// exit publish thread
	s_param.code = code;
}

static void rtmp_onconnect(void*, int code, aio_socket_t aio)
{
	assert(0 == code);

	struct aio_rtmp_client_handler_t handler;
	memset(&handler, 0, sizeof(handler));
	handler.onerror = rtmp_client_publish_onerror;
	handler.onready = rtmp_client_publish_onready;
	handler.onsend = rtmp_client_publish_onsend;

	s_param.code = 0;
	s_param.rtmp = aio_rtmp_client_create(aio, s_param.app, s_param.stream, s_param.tcurl, &handler, NULL);
	assert(0 == aio_rtmp_client_start(s_param.rtmp, 0));
}

// rtmp://video-center.alivecdn.com/live/hello?vhost=your.domain
// rtmp_publish_aio_test("video-center.alivecdn.com", "live", "hello?vhost=your.domain", local-flv-file-name)
void rtmp_publish_aio_test(const char* host, const char* app, const char* stream, const char* file)
{
	s_param.file = file;
	s_param.app = app;
	s_param.stream = stream;
	snprintf(s_param.tcurl, sizeof(s_param.tcurl), "rtmp://%s/%s", host, app); // tcurl

	aio_socket_init(1);
	aio_connect(host, 1935, 3000, rtmp_onconnect, NULL);

	while (aio_socket_process(5000) > 0)
	{
		aio_timeout_process();
	}

	aio_socket_clean();
}
