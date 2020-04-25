#include "aio-worker.h"
#include "dash-mpd.h"
#include "dash-proto.h"
#include "flv-proto.h"
#include "flv-reader.h"
#include "flv-parser.h"
#include "mov-format.h"
#include "http-server.h"
#include "http-route.h"
#include "mpeg4-aac.h"
#include "mpeg4-avc.h"
#include "mpeg4-hevc.h"
#include "cstringext.h"
#include "sys/sync.hpp"
#include "sys/thread.h"
#include "sys/system.h"
#include "sys/path.h"
#include "cpm/shared_ptr.h"
#include "app-log.h"
#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <map>

#define LOCALPATH "./"

struct dash_playlist_t
{
    std::string name;
	dash_mpd_t* mpd;

    int width;
    int height;
	int64_t timestamp;
    int adapation_video;
	int adapation_audio;
	char playlist[256 * 1024];
	uint8_t packet[2 * 1024 * 1024];

    std::list<std::string> files;
};

static ThreadLocker s_locker;

extern "C" const struct mov_buffer_t* mov_file_buffer(void);

static int dash_mpd_onsegment(void* param, int /*track*/, const void* data, size_t bytes, int64_t /*pts*/, int64_t /*dts*/, int64_t /*duration*/, const char* name)
{
    app_log(LOG_DEBUG, "dash_mpd_onsegment %s\n", name);
	FILE* fp = fopen(name, "wb");
    if(fp)
    {
        fwrite(data, 1, bytes, fp);
        fclose(fp);
    }

    dash_playlist_t* dash = (dash_playlist_t*)param;
    if(!strendswith(name, "-init.m4v") && !strendswith(name, "-init.m4a"))
        dash->files.push_back(name);
    while (dash->files.size() > 20)
    {
        app_log(LOG_DEBUG, "Delete %s\n", dash->files.front().c_str());
        path_rmfile(dash->files.front().c_str());
        dash->files.pop_front();
    }

    AutoThreadLocker locker(s_locker);
    dash_mpd_playlist(dash->mpd, dash->playlist, sizeof(dash->playlist));
	return 0;
}

static int dash_live_onflv(void* param, int codec, const void* data, size_t bytes, uint32_t pts, uint32_t dts, int flags)
{
    struct mpeg4_aac_t aac;
    struct mpeg4_avc_t avc;
    struct mpeg4_hevc_t hevc;
    dash_playlist_t* dash = (dash_playlist_t*)param;

    switch (codec)
    {
    case FLV_VIDEO_AVCC:
        if (-1 == dash->adapation_video && mpeg4_avc_decoder_configuration_record_load((const uint8_t*)data, bytes, &avc) > 0)
            dash->adapation_video = dash_mpd_add_video_adaptation_set(dash->mpd, dash->name.c_str(), MOV_OBJECT_H264, dash->width, dash->height, data, bytes);
        break;

    case FLV_VIDEO_HVCC:
        if (-1 == dash->adapation_video && mpeg4_hevc_decoder_configuration_record_load((const uint8_t*)data, bytes, &hevc) > 0)
            dash->adapation_video = dash_mpd_add_video_adaptation_set(dash->mpd, dash->name.c_str(), MOV_OBJECT_HEVC, dash->width, dash->height, data, bytes);
        break;

    case FLV_AUDIO_ASC:
        if (-1 == dash->adapation_audio && mpeg4_aac_audio_specific_config_load((const uint8_t*)data, bytes, &aac) > 0)
        {
            int rate = mpeg4_aac_audio_frequency_to((enum mpeg4_aac_frequency)aac.sampling_frequency_index);
            dash->adapation_audio = dash_mpd_add_audio_adaptation_set(dash->mpd, dash->name.c_str(), MOV_OBJECT_AAC, aac.channel_configuration, 32, rate, data, bytes);
        }
        break;

    case FLV_AUDIO_AAC:
        return dash_mpd_input(dash->mpd, dash->adapation_audio, data, bytes, pts, dts, 0);

    case FLV_VIDEO_H264:
        return dash_mpd_input(dash->mpd, dash->adapation_video, data, bytes, pts, dts, flags ? MOV_AV_FLAG_KEYFREAME : 0);

    case FLV_VIDEO_H265:
        return dash_mpd_input(dash->mpd, dash->adapation_video, data, bytes, pts, dts, flags ? MOV_AV_FLAG_KEYFREAME : 0);

    default:
        assert(0);
    }
    return 0;
}

static int dash_live_worker(const char* file, dash_playlist_t* dash)
{
    int r, type;
    int avcrecord = 0;
    int aacconfig = 0;
    uint32_t timestamp;
    uint32_t s_timestamp = 0;
    uint32_t diff = 0;
    uint64_t clock;

    while (1)
    {
        void* f = flv_reader_create(file);

        clock = system_clock(); // timestamp start from 0
        while ((r = flv_reader_read(f, &type, &timestamp, dash->packet, sizeof(dash->packet))) > 0)
        {
			uint64_t t = system_clock();
			if (clock + timestamp > t && clock + timestamp < t + 3 * 1000)
				system_sleep(clock + timestamp - t);
			else if (clock + timestamp > t + 3 * 1000)
				clock = t - timestamp;

            timestamp += diff;
            s_timestamp = timestamp > s_timestamp ? timestamp : s_timestamp;
            r = flv_parser_input(type, dash->packet, r, timestamp, dash_live_onflv, dash);
            if (0 != r)
            {
                assert(0);
                break; // TODO: handle send failed
            }
        }

        flv_reader_destroy(f);

        diff = s_timestamp + 30;
    }
}

static int dash_server_mpd(http_session_t* session, dash_playlist_t* dash)
{
	http_server_set_header(session, "Content-Type", "application/xml+dash");
	http_server_set_header(session, "Access-Control-Allow-Origin", "*");
	http_server_set_header(session, "Access-Control-Allow-Methods", "GET, POST, PUT");
    AutoThreadLocker locker(s_locker);
	http_server_reply(session, 200, dash->playlist, strlen(dash->playlist));
	return 0;
}

static int dash_server_onlive(void* dash, http_session_t* session, const char* /*method*/, const char* path)
{
    char fullpath[1024];
    int r = path_concat(path + 6 /* /live/ */, LOCALPATH, fullpath);
	printf("live: %s\n", fullpath);

	const char* name = path_basename(fullpath);
	if (strendswith(name, ".mpd"))
	{
        return dash_server_mpd(session, (dash_playlist_t*)dash);
	}
	else if (path_testfile(name))
	{
		// cross domain
		http_server_set_header(session, "Access-Control-Allow-Origin", "*");
		http_server_set_header(session, "Access-Control-Allow-Methods", "GET, POST, PUT");
		return http_server_sendfile(session, name, NULL, NULL);
	}

	return http_server_send(session, 404, "", 0, NULL, NULL);
}

static int dash_server_onvod(void* /*dash*/, http_session_t* session, const char* /*method*/, const char* path)
{
    char fullpath[1024];
    int r = path_concat(path + 5 /* /vod/ */, LOCALPATH, fullpath);
	printf("vod: %s\n", fullpath);

	if (0 == r && path_testfile(fullpath))
	{
		// MIME
		if (strendswith(fullpath, ".mpd"))
			http_server_set_content_type(session, "application/xml+dash");
		else if (strendswith(fullpath, ".mp4") || strendswith(fullpath, ".m4v"))
            http_server_set_header(session, "content-type", "video/mp4");
        else if (strendswith(fullpath, ".m4a"))
            http_server_set_header(session, "content-type", "audio/mp4");

		//http_server_set_header(session, "Transfer-Encoding", "chunked");

		// cross domain
		http_server_set_header(session, "Access-Control-Allow-Origin", "*");
		http_server_set_header(session, "Access-Control-Allow-Methods", "GET, POST, PUT");

		return http_server_sendfile(session, fullpath, NULL, NULL);
	}

	return http_server_send(session, 404, "", 0, NULL, NULL);
}

void dash_dynamic_test(const char* ip, int port, const char* file, int width, int height)
{
    std::shared_ptr<dash_playlist_t> live(new dash_playlist_t());
    live->mpd = dash_mpd_create(DASH_DYNAMIC, dash_mpd_onsegment, live.get());
    live->name = "live";
    live->width = width;
    live->height = height;
    live->adapation_audio = live->adapation_video = -1;

    aio_worker_init(4);
	http_server_t* http = http_server_create(ip, port);
	http_server_set_handler(http, http_server_route, live.get());
	http_server_addroute("/live/", dash_server_onlive);
	http_server_addroute("/vod/", dash_server_onvod);

    // live worker
    dash_live_worker(file, live.get());
    
	http_server_destroy(http);
    aio_worker_clean(4);

    dash_mpd_destroy(live->mpd);
}
