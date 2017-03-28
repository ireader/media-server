#include "rtmp-client.h"
#include "flv-reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "../../../sdk/include/sockutil.h"
#include "../../../sdk/include/sys/system.h"

static char packet[2 * 1024 * 1024];

static int rtmp_sender_send(void* param, const void* data, size_t bytes)
{
	socket_t* socket = (socket_t*)param;
	return socket_send(*socket, data, bytes, 0);
}

static void rtmp_sender_onerror(void* /*param*/, int code, const char* msg)
{
	printf("rtmp_sender_onerror code: %d, msg: %s\n", code, msg);
}

static void rtmp_client_push(const char* flv, void* rtmp)
{
	int r, type;
	uint32_t timestamp;
	static uint32_t s_timestamp = 0;
	void* f = flv_reader_create(flv);
	while ((r = flv_reader_read(f, &type, &timestamp, packet, sizeof(packet))) > 0)
	{
		if(timestamp > s_timestamp)
			system_sleep(timestamp - s_timestamp);
		s_timestamp = timestamp;

		if (8 == type)
		{
			rtmp_client_push_audio(rtmp, packet, r, timestamp);
		}
		else if (9 == type)
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

void rtmp_sender_test(const char* flv, const char* url)
{
	struct rtmp_client_handler_t handler;
	memset(&handler, 0, sizeof(handler));
	handler.onerror = rtmp_sender_onerror;
	handler.send = rtmp_sender_send;

	socket_init();
	socket_t socket = socket_connect_host("192.168.31.129", 1935, 2000);

	void* rtmp = rtmp_client_create("live", "hello", "rtmp://192.168.31.129:1935/live", &socket, &handler);
	int r = rtmp_client_start(rtmp, 0);
	while ((r = socket_recv_by_time(socket, packet, sizeof(packet), 0, 2000)) > 0)
	{
		r = rtmp_client_input(rtmp, packet, r);
	}

	rtmp_client_push(flv, rtmp);

	rtmp_client_destroy(rtmp);
	socket_close(socket);
	socket_cleanup();
}
