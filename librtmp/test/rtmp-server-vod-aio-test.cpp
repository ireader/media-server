#include "sockutil.h"
#include "sys/atomic.h"
#include "sys/thread.h"
#include "sys/system.h"
#include "sys/sync.hpp"
#include "aio-worker.h"
#include "flv-reader.h"
#include "flv-proto.h"
#include "aio-rtmp-server.h"
#include <string.h>
#include <assert.h>

static const char* s_file;

struct rtmp_server_vod_t
{
	int ref;
	ThreadLocker locker;
    aio_rtmp_session_t* session;

    uint8_t packet[4 * 1024 * 1024];
};

static int STDCALL aio_rtmp_server_worker(void* param)
{
	int r, type;
    uint32_t timestamp;
    uint32_t s_timestamp = 0;
    uint32_t diff = 0;
    uint64_t clock;
	//uint64_t clock0 = system_clock() - 3000; // send more data, open fast
	rtmp_server_vod_t* vod = (rtmp_server_vod_t*)param;

    while (1)
    {
        void* f = flv_reader_create(s_file);

        clock = system_clock(); // timestamp start from 0
        while ((r = flv_reader_read(f, &type, &timestamp, vod->packet, sizeof(vod->packet))) > 0)
        {
            assert(r < sizeof(vod->packet));
            uint64_t t = system_clock();
            if (clock + timestamp > t && clock + timestamp < t + 3 * 1000)
                system_sleep(clock + timestamp - t);
			else if (clock + timestamp > t + 3 * 1000)
				clock = t - timestamp;

            timestamp += diff;
            s_timestamp = timestamp > s_timestamp ? timestamp : s_timestamp;

            AutoThreadLocker locker(vod->locker);
            if (NULL == vod->session)
                break;

            while (aio_rtmp_server_get_unsend(vod->session) > 8 * 1024 * 1024)
            {
                vod->locker.Unlock();
                system_sleep(1000); // can't send?
                vod->locker.Lock();
            }

            if (FLV_TYPE_AUDIO == type)
            {
                r = aio_rtmp_server_send_audio(vod->session, vod->packet, r, timestamp);
            }
            else if (FLV_TYPE_VIDEO == type)
            {
                r = aio_rtmp_server_send_video(vod->session, vod->packet, r, timestamp);
            }
            else if (FLV_TYPE_SCRIPT == type)
            {
                r = aio_rtmp_server_send_script(vod->session, vod->packet, r, timestamp);
            }
            else
            {
                //assert(0);
                r = 0;
            }

            if (0 != r)
            {
                assert(0);
                break; // TODO: handle send failed
            }
        }

        flv_reader_destroy(f);

        diff = s_timestamp + 30;
    }

	if(0 == atomic_decrement32(&vod->ref))
		delete vod;
	return 0;
}

static aio_rtmp_userptr_t aio_rtmp_server_onplay(void* /*param*/, aio_rtmp_session_t* session, const char* app, const char* stream, double start, double duration, uint8_t reset)
{
	printf("aio_rtmp_server_onplay(%s, %s, %f, %f, %d)\n", app, stream, start, duration, (int)reset);

	rtmp_server_vod_t* vod = new rtmp_server_vod_t;
	vod->session = session;
	vod->ref = 2;

	pthread_t t;
	thread_create(&t, aio_rtmp_server_worker, vod);
	thread_detach(t);
	return vod;
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

static int aio_rtmp_server_ongetduration(void* param, const char* app, const char* stream, double* duration)
{
	*duration = 30 * 60;
	return 0;
}

static void aio_rtmp_server_onsend(aio_rtmp_userptr_t /*ptr*/, size_t /*bytes*/)
{
}

static void aio_rtmp_server_onclose(aio_rtmp_userptr_t ptr)
{
	// close thread

	rtmp_server_vod_t* vod = (rtmp_server_vod_t*)ptr;
	{
		AutoThreadLocker locker(vod->locker);
		vod->session = NULL;
	}

	if (0 == atomic_decrement32(&vod->ref))
		delete vod;
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
	handler.ongetduration = aio_rtmp_server_ongetduration;

	aio_worker_init(8);

	s_file = flv;
	rtmp = aio_rtmp_server_create(NULL, 1935, &handler, NULL);

	while ('q' != getchar())
	{
	}

	aio_rtmp_server_destroy(rtmp);
    aio_worker_clean(8);
}
