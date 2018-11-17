#include "rtmp-server.h"
#include "sys/thread.h"
#include "sys/system.h"
#include "sys/path.h"
#include <string.h>
#include <assert.h>
#include <vector>

#define N 5000
#define M 8

struct rtmp_server_publish_benchmark_t
{
    rtmp_server_t* rtmp;
    size_t i;
};

struct rtmp_raw_packet_t
{
    const uint8_t* data;
    uint32_t size;
    uint32_t timestamp;
};

static uint64_t s_clock;
static std::vector<rtmp_raw_packet_t> s_pkts;
static rtmp_server_publish_benchmark_t s_servers[N];

static void load_file(const char* filename, void** p, int64_t* n)
{
    *n = path_filesize(filename);
    *p = malloc(*n);
    FILE* fp = fopen(filename, "rb");
    fread(*p, 1, *n, fp);
    fclose(fp);
}
static void init_packets(const char* filename)
{
    // TODO: free(p)
    int64_t n;
    uint8_t* p;
    load_file(filename, (void**)&p, &n);
    for (int64_t i = 0; i < n; i += 8)
    {
        struct rtmp_raw_packet_t pkt;
        pkt.size = *(uint32_t*)(p + i);
        pkt.timestamp = *(uint32_t*)(p + i + 4);
        pkt.data = p + i + 8;
        i += pkt.size;
        s_pkts.push_back(pkt);
    }
}

static void rtmp_server_publish_input(rtmp_server_publish_benchmark_t* rtmp, uint64_t now)
{
    if (rtmp->i >= s_pkts.size())
        return;

    for(int i = 0; 1; i++)
    {
        rtmp_raw_packet_t& pkt = s_pkts[rtmp->i];
        if (s_clock + pkt.timestamp > now)
        {
            if (i > 50) printf("cycle %d\n", i);
            return;
        }

        rtmp_server_input(rtmp->rtmp, pkt.data, pkt.size);
        ++rtmp->i;
    }
}

static int STDCALL rtmp_server_onthread(void* param)
{
    int idx = (int)(intptr_t)param;
    while (1)
    {
        uint64_t now = system_clock();
        for (int i = idx; i < N; i += M)
        {
            rtmp_server_publish_input(s_servers + i, now);
        }
        system_sleep(10);
    }
    return 0;
}

static int rtmp_server_send(void* param, const void* header, size_t len, const void* data, size_t bytes)
{
    return len + bytes;
}

static int rtmp_server_onpublish(void* param, const char* app, const char* stream, const char* type)
{
    return 0;
}
static int rtmp_server_onscript(void* param, const void* script, size_t bytes, uint32_t timestamp)
{
    return 0;
}
static int rtmp_server_onvideo(void* param, const void* data, size_t bytes, uint32_t timestamp)
{
    return 0;
}
static int rtmp_server_onaudio(void* param, const void* data, size_t bytes, uint32_t timestamp)
{
    return 0;
}
void rtmp_server_publish_benchmark_test(const char* bin)
{
    init_packets(bin);

    for (int i = 0; i < N; i++)
    {
        struct rtmp_server_handler_t handler;
        memset(&handler, 0, sizeof(handler));
        handler.send = rtmp_server_send;
        handler.onpublish = rtmp_server_onpublish;
        handler.onscript = rtmp_server_onscript;
        handler.onvideo = rtmp_server_onvideo;
        handler.onaudio = rtmp_server_onaudio;
        s_servers[i].rtmp = rtmp_server_create(NULL, &handler);
        s_servers[i].i = 0;
    }

    s_clock = system_clock();
    for (int i = 0; i < M; i++)
    {
        // TODO destroy thread
        pthread_t thread;
        thread_create(&thread, rtmp_server_onthread, (void*)i);
    }
    
    assert(s_pkts.size() > 0);
    uint32_t duration = s_pkts.rbegin()->timestamp;
    system_sleep(duration + 100);

    for (int i = 0; i < N; i++)
    {
        rtmp_server_destroy(s_servers[i].rtmp);
    }
}
