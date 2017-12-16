#include "sys/sock.h"
#include "sys/system.h"
#include "flv-writer.h"
#include "flv-proto.h"
#include "aio-connect.h"
#include "aio-rtmp-client.h"
#include <stdio.h>
#include <assert.h>

static struct
{
	int code;
	char tcurl[4 * 1024];
	const char* app;
	const char* stream;
	aio_rtmp_client_t* rtmp;
} s_param;

static void rtmp_client_play_onerror(void* /*flv*/, int code)
{
	s_param.code = code;
}

static int rtmp_client_play_onvideo(void* flv, const void* video, size_t bytes, uint32_t timestamp)
{
	return flv_writer_input(flv, FLV_TYPE_VIDEO, video, bytes, timestamp);
}

static int rtmp_client_play_onaudio(void* flv, const void* audio, size_t bytes, uint32_t timestamp)
{
	return flv_writer_input(flv, FLV_TYPE_AUDIO, audio, bytes, timestamp);
}

static int rtmp_client_play_onscript(void* flv, const void* script, size_t bytes, uint32_t timestamp)
{
	return flv_writer_input(flv, FLV_TYPE_SCRIPT, script, bytes, timestamp);
}

static void rtmp_onconnect(void* flv, int code, aio_socket_t aio)
{
	assert(0 == code);

	struct aio_rtmp_client_handler_t handler;
	memset(&handler, 0, sizeof(handler));
	handler.onerror = rtmp_client_play_onerror;
	handler.onaudio = rtmp_client_play_onaudio;
	handler.onvideo = rtmp_client_play_onvideo;
	handler.onscript = rtmp_client_play_onscript;
	
	s_param.rtmp = aio_rtmp_client_create(aio, s_param.app, s_param.stream, s_param.tcurl, &handler, flv);
	assert(0 == aio_rtmp_client_start(s_param.rtmp, 1));
}

// rtmp://live.alivecdn.com/live/hello?key=xxxxxx
// rtmp_play_aio_test("live.alivecdn.com", "live", "hello?key=xxxxxx", save-to-local-flv-file-name)
void rtmp_play_aio_test(const char* host, const char* app, const char* stream, const char* file)
{
	s_param.code = 0;
	s_param.app = app;
	s_param.stream = stream;
	snprintf(s_param.tcurl, sizeof(s_param.tcurl), "rtmp://%s/%s", host, app); // tcurl
	
	aio_socket_init(1);
	void* flv = flv_writer_create(file);
	aio_connect(host, 1935, 3000, rtmp_onconnect, flv);
		
//	uint64_t clock = system_clock();
	while (0 == s_param.code)
	{
		aio_socket_process(1000);
	}

	aio_rtmp_client_destroy(s_param.rtmp);
	flv_writer_destroy(flv);
	aio_socket_clean();
}
