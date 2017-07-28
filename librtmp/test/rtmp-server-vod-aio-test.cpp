#include "sockutil.h"
#include "sys/thread.h"
#include "sys/system.h"
#include "aio-socket.h"
#include "flv-reader.h"
#include "flv-proto.h"
#include "aio-rtmp-server.h"
#include <string.h>
#include <assert.h>

static const char* s_file;

static int STDCALL aio_rtmp_server_worker(void* param)
{
	int r, type;
	uint32_t timestamp;
	uint64_t clock0 = system_clock();
	aio_rtmp_session_t* session = (aio_rtmp_session_t*)param;
	void* f = flv_reader_create(s_file);

	static unsigned char packet[8 * 1024 * 1024];
	while ((r = flv_reader_read(f, &type, &timestamp, packet, sizeof(packet))) > 0)
	{
		assert(r < sizeof(packet));
		uint64_t clock = system_clock();
		if (clock0 + timestamp > clock)
			system_sleep(clock0 + timestamp - clock);

		while (aio_rtmp_server_get_unsend(session) > 8 * 1024 * 1024)
			system_sleep(1000); // can't send?

		if (FLV_TYPE_AUDIO == type)
		{
			aio_rtmp_server_send_audio(session, packet, r, timestamp);
		}
		else if (FLV_TYPE_VIDEO == type)
		{
			aio_rtmp_server_send_video(session, packet, r, timestamp);
		}
		else
		{
			assert(0);
		}
	}

	flv_reader_destroy(f);
	return 0;
}

static aio_rtmp_userptr_t aio_rtmp_server_onplay(void* /*param*/, aio_rtmp_session_t* session, const char* app, const char* stream, double start, double duration, uint8_t reset)
{
	printf("aio_rtmp_server_onplay(%s, %s, %f, %f, %d)\n", app, stream, start, duration, (int)reset);

	pthread_t t;
	thread_create(&t, aio_rtmp_server_worker, session);
	thread_detach(t);
	return session;
}

static int aio_rtmp_server_onpause(aio_rtmp_userptr_t /*ptr*/, int pause, uint32_t ms)
{
	printf("aio_rtmp_server_onpause(%d, %u)\n", pause, (unsigned int)ms);
	return 0;
}

static int aio_rtmp_server_onseek(aio_rtmp_userptr_t /*ptr*/, uint32_t ms)
{
	printf("aio_rtmp_server_onseek(%u)\n", (unsigned int)ms);
	return 0;
}

static void aio_rtmp_server_onsend(aio_rtmp_userptr_t /*ptr*/, int /*code*/, size_t /*bytes*/)
{
}

static void aio_rtmp_server_onclose(aio_rtmp_userptr_t /*ptr*/)
{
	// close thread
}

void rtmp_server_vod_aio_test(const char* flv)
{
	aio_rtmp_server_t* rtmp;
	struct aio_rtmp_server_handler_t handler;
	memset(&handler, 0, sizeof(handler));
	handler.onsend = aio_rtmp_server_onsend;
	handler.onplay = aio_rtmp_server_onplay;
	handler.onpause = aio_rtmp_server_onpause;
	handler.onseek = aio_rtmp_server_onseek;
	handler.onclose = aio_rtmp_server_onclose;

	aio_socket_init(1);

	s_file = flv;
	rtmp = aio_rtmp_server_create(NULL, 1935, &handler, NULL);

	while (1)
	{
		aio_socket_process(2000);
	}

	aio_rtmp_server_destroy(rtmp);
	aio_socket_clean();
}
