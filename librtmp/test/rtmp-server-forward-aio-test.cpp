//
// TODO: add packet queue for player
//

#include "aio-rtmp-server.h"
#include "aio-timeout.h"
#include "aio-worker.h"
#include "sys/sync.hpp"
#include "flv-writer.h"
#include "flv-proto.h"
#include "flv-muxer.h"
#include "flv-demuxer.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include "cpm/shared_ptr.h"
#include <string>
#include <list>
#include <map>

struct rtmp_player_t
{
    // TODO: add packet queue
    aio_rtmp_session_t* rtmp;
    struct flv_muxer_t* muxer;

    rtmp_player_t(aio_rtmp_session_t* rtmp) : rtmp(rtmp)
    {
        muxer = flv_muxer_create(&handler, this);
    }

    ~rtmp_player_t()
    {
        if(muxer)
            flv_muxer_destroy(muxer);
    }

private:
    static int handler(void* param, int type, const void* data, size_t bytes, uint32_t timestamp)
    {
        rtmp_player_t* player = (rtmp_player_t*)param;
        switch (type)
        {
        case FLV_TYPE_SCRIPT:
            return aio_rtmp_server_send_script(player->rtmp, data, bytes, timestamp);
        case FLV_TYPE_AUDIO:
            return aio_rtmp_server_send_audio(player->rtmp, data, bytes, timestamp);
        case FLV_TYPE_VIDEO:
            return aio_rtmp_server_send_video(player->rtmp, data, bytes, timestamp);
        default:
            assert(0);
            return -1;
        }
    }
};

struct rtmp_source_t
{
    ThreadLocker locker;
    struct flv_demuxer_t* demuxer;
    std::list<std::shared_ptr<rtmp_player_t> > players;

    rtmp_source_t()
    {
        demuxer = flv_demuxer_create(handler, this);
    }

    ~rtmp_source_t()
    {
        if (demuxer)
            flv_demuxer_destroy(demuxer);
    }

private:
    static int handler(void* param, int codec, const void* data, size_t bytes, uint32_t pts, uint32_t dts, int flags)
    {
        int r = 0;
        rtmp_source_t* s = (rtmp_source_t*)param;

        AutoThreadLocker locker(s->locker);
        for (auto it = s->players.begin(); it != s->players.end(); ++it)
        {
            // TODO: push to packet queue

            switch (codec)
            {
            case FLV_VIDEO_H264:
                r = flv_muxer_avc((*it)->muxer, data, bytes, pts, dts);
                break;
            case FLV_VIDEO_H265:
                r = flv_muxer_hevc((*it)->muxer, data, bytes, pts, dts);
                break;
            case FLV_AUDIO_AAC:
                r = flv_muxer_aac((*it)->muxer, data, bytes, pts, dts);
                break;
            case FLV_AUDIO_MP3:
                r = flv_muxer_mp3((*it)->muxer, data, bytes, pts, dts);
                break;

            case FLV_VIDEO_AVCC:
            case FLV_VIDEO_HVCC:
            case FLV_AUDIO_ASC:
                break; // ignore

            default:
                assert(0);
            }
        }
        return 0; // ignore error
    }
};

static ThreadLocker s_locker;
static std::map<std::string, std::shared_ptr<rtmp_source_t> > s_lives;

static aio_rtmp_userptr_t aio_rtmp_server_onpublish(void* param, aio_rtmp_session_t* /*session*/, const char* app, const char* stream, const char* type)
{
    printf("aio_rtmp_server_onpublish(%s, %s, %s)\n", app, stream, type);
    std::string key(app);
    key += "/";
    key += stream;

    std::shared_ptr<rtmp_source_t> source(new rtmp_source_t);
    AutoThreadLocker locker(s_locker);
    assert(s_lives.find(key) == s_lives.end());
    s_lives[key] = source;
    return source.get();
}

static int aio_rtmp_server_onscript(aio_rtmp_userptr_t ptr, const void* script, size_t bytes, uint32_t timestamp)
{
    struct rtmp_source_t* s = (struct rtmp_source_t*)ptr;
    AutoThreadLocker locker(s->locker);
    return flv_demuxer_input(s->demuxer, FLV_TYPE_SCRIPT, script, bytes, timestamp);
}

static int aio_rtmp_server_onvideo(aio_rtmp_userptr_t ptr, const void* data, size_t bytes, uint32_t timestamp)
{
    struct rtmp_source_t* s = (struct rtmp_source_t*)ptr;
    AutoThreadLocker locker(s->locker);
    return flv_demuxer_input(s->demuxer, FLV_TYPE_VIDEO, data, bytes, timestamp);
}

static int aio_rtmp_server_onaudio(aio_rtmp_userptr_t ptr, const void* data, size_t bytes, uint32_t timestamp)
{
    struct rtmp_source_t* s = (struct rtmp_source_t*)ptr;
    AutoThreadLocker locker(s->locker);
    return flv_demuxer_input(s->demuxer, FLV_TYPE_AUDIO, data, bytes, timestamp);
}

static void aio_rtmp_server_onsend(aio_rtmp_userptr_t /*ptr*/, size_t /*bytes*/)
{
}

static void aio_rtmp_server_onclose(aio_rtmp_userptr_t ptr)
{
    AutoThreadLocker locker(s_locker);
    for (auto it = s_lives.begin(); it != s_lives.end(); ++it)
    {
        std::shared_ptr<struct rtmp_source_t>& s = it->second;
        if (ptr == s.get())
        {
            s_lives.erase(it);
            return;
        }

        AutoThreadLocker l(s->locker);
        for (auto j = s->players.begin(); j != s->players.end(); ++j)
        {
            if (j->get() == ptr)
            {
                s->players.erase(j);
                return;
            }
        }
    }
}

static aio_rtmp_userptr_t aio_rtmp_server_onplay(void* /*param*/, aio_rtmp_session_t* session, const char* app, const char* stream, double start, double duration, uint8_t reset)
{
    printf("aio_rtmp_server_onplay(%s, %s, %f, %f, %d)\n", app, stream, start, duration, (int)reset);
    std::string key(app);
    key += "/";
    key += stream;

    std::shared_ptr<struct rtmp_source_t> s;
    {
        AutoThreadLocker locker(s_locker);
        auto it = s_lives.find(key);
        if (it == s_lives.end())
        {
            printf("source(%s, %s) not found\n", app, stream);
            return NULL;
        }
        s = it->second;
    }
    
    std::shared_ptr<rtmp_player_t> player(new rtmp_player_t(session));
    AutoThreadLocker locker(s->locker);
    s->players.push_back(player);
    return player.get();
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

void rtmp_server_forward_aio_test(const char* ip, int port)
{
    aio_rtmp_server_t* rtmp;
    struct aio_rtmp_server_handler_t handler;
    memset(&handler, 0, sizeof(handler));
    handler.onsend = aio_rtmp_server_onsend;
    handler.onplay = aio_rtmp_server_onplay;
    handler.onpause = aio_rtmp_server_onpause;
    handler.onseek = aio_rtmp_server_onseek;
    handler.onpublish = aio_rtmp_server_onpublish;
    handler.onscript = aio_rtmp_server_onscript;
    handler.onaudio = aio_rtmp_server_onaudio;
    handler.onvideo = aio_rtmp_server_onvideo;
    handler.onclose = aio_rtmp_server_onclose;

    aio_worker_init(8);

    rtmp = aio_rtmp_server_create(ip, port, &handler, NULL);

    while ('q' != getchar())
    {
    }

    aio_rtmp_server_destroy(rtmp);
    aio_worker_clean(8);
}
