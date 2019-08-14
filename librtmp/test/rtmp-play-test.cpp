#include "sockutil.h"
#include "rtmp-client.h"
#include "flv-writer.h"
#include "flv-proto.h"
#include <assert.h>
#include <stdio.h>

static void* s_flv;

static int rtmp_client_send(void* param, const void* header, size_t len, const void* data, size_t bytes)
{
	socket_t* socket = (socket_t*)param;
	socket_bufvec_t vec[2];
	socket_setbufvec(vec, 0, (void*)header, len);
	socket_setbufvec(vec, 1, (void*)data, bytes);
	return socket_send_v_all_by_time(*socket, vec, bytes ? 2 : 1, 0, 2000);
}

static int rtmp_client_onaudio(void* /*param*/, const void* data, size_t bytes, uint32_t timestamp)
{
	return flv_writer_input(s_flv, FLV_TYPE_AUDIO, data, bytes, timestamp);
}

static int rtmp_client_onvideo(void* /*param*/, const void* data, size_t bytes, uint32_t timestamp)
{
	return flv_writer_input(s_flv, FLV_TYPE_VIDEO, data, bytes, timestamp);
}

static int rtmp_client_onscript(void* /*param*/, const void* data, size_t bytes, uint32_t timestamp)
{
	return flv_writer_input(s_flv, FLV_TYPE_SCRIPT, data, bytes, timestamp);
}

// rtmp://live.alivecdn.com/live/hello?key=xxxxxx
// rtmp_publish_aio_test("live.alivecdn.com", "live", "hello?key=xxxxxx", save-to-local-flv-file-name)
void rtmp_play_test(const char* host, const char* app, const char* stream, const char* flv)
{
	static char packet[8 * 1024 * 1024];
	snprintf(packet, sizeof(packet), "rtmp://%s/%s", host, app); // tcurl

	socket_init();
	socket_t socket = socket_connect_host(host, 1935, 2000);
	socket_setnonblock(socket, 0);

	struct rtmp_client_handler_t handler;
	memset(&handler, 0, sizeof(handler));
	handler.send = rtmp_client_send;
	handler.onaudio = rtmp_client_onaudio;
	handler.onvideo = rtmp_client_onvideo;
	handler.onscript = rtmp_client_onscript;
	rtmp_client_t* rtmp = rtmp_client_create(app, stream, packet/*tcurl*/, &socket, &handler);
	s_flv = flv_writer_create(flv);

	int r = rtmp_client_start(rtmp, 1);

	while ((r = socket_recv(socket, packet, sizeof(packet), 0)) > 0)
	{
		assert(0 == rtmp_client_input(rtmp, packet, r));
	}

	rtmp_client_stop(rtmp);
	flv_writer_destroy(s_flv);
	rtmp_client_destroy(rtmp);
	socket_close(socket);
	socket_cleanup();
}
