#include "sys/sock.h"
#include "sys/system.h"
#include "flv-writer.h"
#include "flv-proto.h"
#include "rtmp-client-transport.h"
#include <stdio.h>
#include <assert.h>
#include "time64.h"

static void* rtmp_client_play_alloc(void* /*flv*/, int avtype, size_t bytes)
{
	static uint8_t audio[128 * 1024];
	static uint8_t video[2 * 1024 * 1024];
	assert(avtype || sizeof(audio) > bytes);
	assert(sizeof(video) >= bytes);
	return avtype ? video : audio;
}

static void rtmp_client_play_onvideo(void* flv, const void* video, size_t bytes, uint32_t timestamp)
{
	flv_writer_input(flv, FLV_TYPE_VIDEO, video, bytes, timestamp);
}

static void rtmp_client_play_onaudio(void* flv, const void* audio, size_t bytes, uint32_t timestamp)
{
	flv_writer_input(flv, FLV_TYPE_AUDIO, audio, bytes, timestamp);
}

// rtmp_play_test("rtmp://strtmpplay.cdn.suicam.com/carousel/51632");
void rtmp_play_aio_test(const char* host, const char* app, const char* stream, const char* file)
{
	static char packet[8 * 1024 * 1024];
	snprintf(packet, sizeof(packet), "rtmp://%s/%s/%s", host, app, stream); // tcurl
	
	struct rtmp_client_transport_handler_t handler;
	handler.alloc = rtmp_client_play_alloc;
	handler.onaudio = rtmp_client_play_onaudio; 
	handler.onvideo = rtmp_client_play_onvideo;
	
	void* flv = flv_writer_create(file);
	void* transport = rtmp_client_transport_create(host, 1935, app, stream, packet, &handler, flv);
	rtmp_client_transport_start(transport, 1);

	time64_t clock = time64_now();
	while (clock + 2 * 60 * 1000 > time64_now())
	{
		system_sleep(1000);
	}

	rtmp_client_transport_destroy(transport);
	flv_writer_destroy(flv);
}
