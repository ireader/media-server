#include "aio-rtmp-server.h"
#include "aio-socket.h"
#include "flv-writer.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>

static aio_rtmp_userptr_t aio_rtmp_server_onpublish(void* param, aio_rtmp_session_t* /*session*/, const char* app, const char* stream, const char* type)
{
	printf("aio_rtmp_server_onpublish(%s, %s, %s)\n", app, stream, type);
	
	return flv_writer_create((const char*)param);
}

static int aio_rtmp_server_onvideo(aio_rtmp_userptr_t flv, const void* data, size_t bytes, uint32_t timestamp)
{
	return flv_writer_input(flv, 9, data, bytes, timestamp);
}

static int aio_rtmp_server_onaudio(aio_rtmp_userptr_t flv, const void* data, size_t bytes, uint32_t timestamp)
{
	return flv_writer_input(flv, 8, data, bytes, timestamp);
}

static void aio_rtmp_server_onsend(aio_rtmp_userptr_t /*ptr*/, int /*code*/, size_t /*bytes*/)
{
}

static void aio_rtmp_server_onclose(aio_rtmp_userptr_t flv)
{
	flv_writer_destroy(flv);
}

void rtmp_server_publish_aio_test(const char* flv)
{
	aio_rtmp_server_t* rtmp;
	struct aio_rtmp_server_handler_t handler;
	memset(&handler, 0, sizeof(handler));
	handler.onsend = aio_rtmp_server_onsend;
	handler.onpublish = aio_rtmp_server_onpublish;
	handler.onaudio = aio_rtmp_server_onaudio;
	handler.onvideo = aio_rtmp_server_onvideo;
	handler.onclose = aio_rtmp_server_onclose;

	aio_socket_init(1);

	rtmp = aio_rtmp_server_create(NULL, 1935, &handler, (void*)flv);

	while (1)
	{
		aio_socket_process(2000);
	}

	aio_rtmp_server_destroy(rtmp);
	aio_socket_clean();
}
