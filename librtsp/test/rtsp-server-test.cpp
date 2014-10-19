#if defined(_DEBUG) || defined(DEBUG)
#include "cstringext.h"
#include "sys/sock.h"
#include "sys/system.h"
#include "sys/process.h"
#include "sys/path.h"
#include "aio-socket.h"
#include "thread-pool.h"
#include "rtsp-server.h"
#include "ntp-time.h"
#include "rtp-profile.h"
#include "h264-file-source.h"
#include <map>

static int s_running;
static thread_pool_t s_pool;
static std::map<void*, IMediaSource*> s_sources;

static void worker(void* param)
{
    int r;
    do
    {
        r = aio_socket_process(2*60*1000);
        if(0 != r)
        {
            //printf("http_server_process =>%d\n", r);
        }
    } while(*(int*)param && -1 != r);
}

static int init()
{
    int cpu = (int)system_getcpucount();
    s_pool = thread_pool_create(cpu, 1, 64);
    aio_socket_init(cpu);
    
    s_running = 1;
    while(cpu-- > 0)
    {
        thread_pool_push(s_pool, worker, &s_running); // start worker
    }
    
    return 0;
}

static int cleanup()
{
    s_running = 0;
    thread_pool_destroy(s_pool);
    aio_socket_clean();
    return 0;
}

static void rtsp_ondescribe(void* ptr, void* rtsp, const char* uri)
{
    static const char* pattern =
        "v=0\n"
        "o=- %llu %llu IN IP4 %s\n"
        "s=%s\n"
        "c=IN IP4 0.0.0.0\n"
        "t=0 %lld\n"
        "a=recvonly\n";

    char sdp[512];
    int64_t duration = 0;
    std::string mediasdp;

    IMediaSource *source = NULL;
    source = H264FileSource::Create(uri);
    source->GetDuration(duration);
    source->GetSDPMedia(mediasdp);

    snprintf(sdp, sizeof(sdp), pattern, ntp64_now(), ntp64_now(), "0.0.0.0", uri, duration);
    strcat(sdp, mediasdp.c_str());

    rtsp_server_reply_describe(rtsp, 200, sdp);
}

static void rtsp_onsetup(void* ptr, void* rtsp, const char* uri, const char* session, const struct rtsp_header_transport_t* transport)
{
    const char* rtsp_transport = "";
    rtsp_server_reply_setup(rtsp, 200, session, rtsp_transport);
}

static void rtsp_onplay(void* ptr, void* rtsp, const char* uri, const char* session, const int64_t *npt, const double *scale)
{
    rtsp_server_reply_play(rtsp, 200, 0, 0, NULL);
}

static void rtsp_onpause(void* ptr, void* rtsp, const char* uri, const char* session, const int64_t *npt)
{
    rtsp_server_reply_pause(rtsp, 200);
}

static void rtsp_onteardown(void* ptr, void* rtsp, const char* uri, const char* session)
{
    rtsp_server_reply_teardown(rtsp, 200);
}

void rtsp_benchmark()
{
    void *rtsp;
    struct rtsp_handler_t handler;
    
    rtsp_server_init();
    init();

    handler.describe = rtsp_ondescribe;
    handler.setup = rtsp_onsetup;
    handler.play = rtsp_onplay;
    handler.pause = rtsp_onpause;
    handler.teardown = rtsp_onteardown;
    rtsp = rtsp_server_create(NULL, 554, &handler, NULL);
    
    while(1)
    {
        system_sleep(10000);
    }
    
    rtsp_server_cleanup();
}

#endif
