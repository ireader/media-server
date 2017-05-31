#include "sockutil.h"
#include "sys/system.h"
#include "rtmp-client.h"
#include "flv-reader.h"
#include "flv-proto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int rtmp_client_send(void* param, const void* header, size_t len, const void* data, size_t bytes)
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

static void rtmp_client_push(const char* flv, void* rtmp)
{
	int r, type;
	uint32_t timestamp;
	static uint32_t s_timestamp = 0;
	void* f = flv_reader_create(flv);

	static char packet[2 * 1024 * 1024];
	while ((r = flv_reader_read(f, &type, &timestamp, packet, sizeof(packet))) > 0)
	{
		if(timestamp > s_timestamp)
			system_sleep(timestamp - s_timestamp);
		s_timestamp = timestamp;

		if (FLV_TYPE_AUDIO == type)
		{
			rtmp_client_push_audio(rtmp, packet, r, timestamp);
		}
		else if (FLV_TYPE_VIDEO == type)
		{
			rtmp_client_push_video(rtmp, packet, r, timestamp);
		}
		else
		{
			assert(0);
		}
	}

	flv_reader_destroy(f);
}

void rtmp_publish_test(const char* host, const char* app, const char* stream, const char* flv)
{
	static char packet[2 * 1024 * 1024];
	snprintf(packet, sizeof(packet), "rtmp://%s/%s/%s", host, app, stream); // tcurl

	struct rtmp_client_handler_t handler;
	memset(&handler, 0, sizeof(handler));
	handler.send = rtmp_client_send;

	socket_init();
	socket_t socket = socket_connect_host(host, 1935, 2000);
	socket_setnonblock(socket, 0);

	void* rtmp = rtmp_client_create(app, stream, packet/*tcurl*/, &socket, &handler);
	int r = rtmp_client_start(rtmp, 0);

	while ((r = socket_recv(socket, packet, sizeof(packet), 0)) > 0)
	{
		r = rtmp_client_input(rtmp, packet, r);
	}

	rtmp_client_push(flv, rtmp);

	rtmp_client_destroy(rtmp);
	socket_close(socket);
	socket_cleanup();
}
