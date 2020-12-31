#include "sockutil.h"
#include "rtmp-server.h"
#include "flv-writer.h"
#include "flv-proto.h"
#include "sys/thread.h"
#include "sys/system.h"
#include <string.h>
#include <assert.h>

static void* s_flv;

static int rtmp_server_send(void* param, const void* header, size_t len, const void* data, size_t bytes)
{
	socket_t* socket = (socket_t*)param;
	socket_bufvec_t vec[2];
	socket_setbufvec(vec, 0, (void*)header, len);
	socket_setbufvec(vec, 1, (void*)data, bytes);
	return socket_send_v_all_by_time(*socket, vec, bytes > 0 ? 2 : 1, 0, 2000);
}

static int rtmp_server_onpublish(void* param, const char* app, const char* stream, const char* type)
{
	printf("rtmp_server_onpublish(%s, %s, %s)\n", app, stream, type);
	return 0;
}

static int rtmp_server_onscript(void* param, const void* script, size_t bytes, uint32_t timestamp)
{
	return flv_writer_input(s_flv, FLV_TYPE_SCRIPT, script, bytes, timestamp);
}

static int rtmp_server_onvideo(void* param, const void* data, size_t bytes, uint32_t timestamp)
{
	return flv_writer_input(s_flv, FLV_TYPE_VIDEO, data, bytes, timestamp);
}

static int rtmp_server_onaudio(void* param, const void* data, size_t bytes, uint32_t timestamp)
{
	return flv_writer_input(s_flv, FLV_TYPE_AUDIO, data, bytes, timestamp);
}

void rtmp_server_publish_test(const char* flv)
{
	int r;
	struct rtmp_server_handler_t handler;
	memset(&handler, 0, sizeof(handler));
	handler.send = rtmp_server_send;
	//handler.oncreate_stream = rtmp_server_oncreate_stream;
	//handler.ondelete_stream = rtmp_server_ondelete_stream;
	//handler.onplay = rtmp_server_onplay;
	//handler.onpause = rtmp_server_onpause;
	//handler.onseek = rtmp_server_onseek;
	handler.onpublish = rtmp_server_onpublish;
	handler.onscript = rtmp_server_onscript;
	handler.onvideo = rtmp_server_onvideo;
	handler.onaudio = rtmp_server_onaudio;
	
	socket_init();

	socklen_t n;
	struct sockaddr_storage ss;
	socket_t s = socket_tcp_listen(NULL, 1935, SOMAXCONN);
	socket_t c = socket_accept(s, &ss, &n);

	s_flv = flv_writer_create(flv);
	rtmp_server_t* rtmp = rtmp_server_create(&c, &handler);

	static unsigned char packet[8 * 1024 * 1024];
	while ((r = socket_recv(c, packet, sizeof(packet), 0)) > 0)
	{
		assert(0 == rtmp_server_input(rtmp, packet, r));
	}

	rtmp_server_destroy(rtmp);
	flv_writer_destroy(s_flv);
	socket_close(c);
	socket_close(s);
	socket_cleanup();
}
