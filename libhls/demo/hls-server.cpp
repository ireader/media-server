#include "aio-worker.h"
#include "aio-socket.h"
#include "mpeg-ps.h"
#include "mpeg-ts.h"
#include "hls-m3u8.h"
#include "hls-media.h"
#include "hls-param.h"
#include "flv-proto.h"
#include "flv-reader.h"
#include "flv-demuxer.h"
#include "http-server.h"
#include "http-route.h"
#include "sys/thread.h"
#include "sys/system.h"
#include "sys/path.h"
#include "cstringext.h"
#include "utf8codec.h"
#include <string.h>
#include <assert.h>
#include <map>
#include <list>
#include <atomic>
#include <vector>
#include <string>

#define CWD "d:\\video\\"

extern "C" int http_list_dir(http_session_t* session, const char* path);

struct hls_ts_t
{
    std::atomic<int> ref;
	void* data;
	size_t size;
	std::string name;
};

struct hls_playlist_t
{
	pthread_t t;
	std::string file;

	hls_media_t* hls;
	hls_m3u8_t* m3u8;
	int64_t pts;
	int64_t last_pts;
	uint8_t packet[2 * 1024 * 1024];

	int i;
	std::list<hls_ts_t*> files;
};

static std::map<std::string, hls_playlist_t*> s_playlists;

static int hls_handler(void* param, const void* data, size_t bytes, int64_t pts, int64_t /*dts*/, int64_t duration)
{
	hls_playlist_t* playlist = (hls_playlist_t*)param;

	int discontinue = 0;
	if (playlist->i > 0)
	{
		discontinue = (playlist->last_pts + HLS_DURATION * 1000 < pts/*discontinue*/ || pts + duration + HLS_DURATION * 1000 < playlist->pts/*rewind*/) ? 1 : 0;
	}
	playlist->pts = pts;
	playlist->last_pts = pts + duration;

	char name[128] = { 0 };
	snprintf(name, sizeof(name), "%s/%d.ts", playlist->file.c_str(), playlist->i++);
	hls_m3u8_add(playlist->m3u8, name, pts, duration, discontinue);

	// add new segment
	hls_ts_t* ts = new hls_ts_t;
	ts->ref = 1;
	ts->name = name;
	ts->size = bytes;
	ts->data = malloc(bytes);
	memcpy(ts->data, data, bytes);
	playlist->files.push_back(ts);

	// remove oldest segment
	while(playlist->files.size() > HLS_LIVE_NUM + 1)
	{
		ts = playlist->files.front();
		playlist->files.pop_front();
        if (0 == std::atomic_fetch_sub(&ts->ref, 1) - 1)
		{
			free(ts->data);
			delete ts;
		}
	}

	printf("new segment: %s\n", name);
	return 0;
}

static int flv_handler(void* param, int codec, const void* data, size_t bytes, uint32_t pts, uint32_t dts, int flags)
{
	hls_media_t* hls = (hls_media_t*)param;

	switch (codec)
	{
	case FLV_AUDIO_AAC:
		return hls_media_input(hls, STREAM_AUDIO_AAC, data, bytes, pts, dts, 0);

	case FLV_AUDIO_MP3:
		return hls_media_input(hls, STREAM_AUDIO_MP3, data, bytes, pts, dts, 0);

	case FLV_VIDEO_H264:
		return hls_media_input(hls, STREAM_VIDEO_H264, data, bytes, pts, dts, flags ? HLS_FLAGS_KEYFRAME : 0);

	case FLV_VIDEO_H265:
		return hls_media_input(hls, STREAM_VIDEO_H265, data, bytes, pts, dts, flags ? HLS_FLAGS_KEYFRAME : 0);

	default:
		// nothing to do
		return 0;
	}
}

static int STDCALL hls_server_worker(void* param)
{
	int r, type;
	uint64_t clock;
	uint32_t timestamp;
	hls_playlist_t* playlist = (hls_playlist_t*)param;

	std::string file = playlist->file + ".flv";
	UTF8Decode utf8(file.c_str());
	std::string fullpath = CWD;
	fullpath += utf8;

	while (1)
	{
		void* flv = flv_reader_create(fullpath.c_str());
		flv_demuxer_t* demuxer = flv_demuxer_create(flv_handler, playlist->hls);

		clock = 0;
		while ((r = flv_reader_read(flv, &type, &timestamp, playlist->packet, sizeof(playlist->packet))) > 0)
		{
			uint64_t now = system_clock();
			if (0 == clock)
			{
				clock = now;
			}
			else
			{
				if (timestamp > now - clock)
					system_sleep(timestamp - (now - clock));
			}

			assert(0 == flv_demuxer_input(demuxer, type, playlist->packet, r, timestamp));
		}

		flv_demuxer_destroy(demuxer);
		flv_reader_destroy(flv);
	}
	hls_media_destroy(playlist->hls);
	//hls_m3u8_destroy(playlist->m3u8);
	//s_playlists.erase();
	//delete playlist;
	return thread_destroy(playlist->t);
}

static int hls_server_m3u8(http_session_t* session, const std::string& path)
{
	char playlist[8 * 1024];
	hls_m3u8_t* m3u8 = s_playlists.find(path)->second->m3u8;
	assert(m3u8);
	assert(0 == hls_m3u8_playlist(m3u8, 0, playlist, sizeof(playlist)));
	
	http_server_set_header(session, "content-type", HLS_M3U8_TYPE);
	http_server_set_header(session, "Access-Control-Allow-Origin", "*");
	http_server_set_header(session, "Access-Control-Allow-Methods", "GET, POST, PUT");
	http_server_reply(session, 200, playlist, strlen(playlist));

	printf("load %s.m3u8 file\n", path.c_str());
	return 0;
}

static int hls_server_ts_onsend(void* param, int code, size_t bytes)
{
	hls_ts_t* ts = (hls_ts_t*)param;
    if (0 == std::atomic_fetch_sub(&ts->ref, 1) - 1)
	{
		free(ts->data);
		delete ts;
	}
	return 0;
}

static int hls_server_ts(http_session_t* session, const std::string& path, const std::string& ts)
{
	hls_playlist_t* playlist = s_playlists.find(path)->second;
	assert(playlist);

	std::list<hls_ts_t*>::iterator i;
	std::string file = path + '/' + ts;
	for(i = playlist->files.begin(); i != playlist->files.end(); ++i)
	{
		hls_ts_t* ts = *i;
		if(ts->name == file)
		{
            std::atomic_fetch_add(&ts->ref, 1);
			http_server_set_header(session, "Access-Control-Allow-Origin", "*");
			http_server_set_header(session, "Access-Control-Allow-Methods", "GET, POST, PUT");
			http_server_send(session, 200, ts->data, ts->size, hls_server_ts_onsend, ts);
			printf("load file %s\n", file.c_str());
			return 0;
		}
	}

	printf("load ts file(%s) failed\n", file.c_str());
	return http_server_send(session, 404, "", 0, NULL, NULL);
}

static int hls_server_onlive(void* /*http*/, http_session_t* session, const char* /*method*/, const char* path)
{
	path = path + 6;
	if (strendswith(path, ".m3u8"))
	{
		std::string app(path, strlen(path) - 5);
		if (s_playlists.find(app) == s_playlists.end())
		{
			hls_playlist_t* playlist = new hls_playlist_t();
			playlist->file = app;
			playlist->m3u8 = hls_m3u8_create(HLS_LIVE_NUM, 3);
			playlist->hls = hls_media_create(HLS_DURATION * 1000, hls_handler, playlist);
			playlist->i = 0;
			s_playlists[app] = playlist;

			thread_create(&playlist->t, hls_server_worker, playlist);
		}

		return hls_server_m3u8(session, app);
	}
	else if (strendswith(path, ".ts"))
	{
		const char* ts = strchr(path, '/');
		std::string app(path, ts ? ts - path : strlen(path));
		if (ts && s_playlists.find(app) != s_playlists.end())
		{
			return hls_server_ts(session, app, ts + 1);
		}
	}

	return http_server_send(session, 404, "", 0, NULL, NULL);
}

static int hls_server_onvod(void* /*http*/, http_session_t* session, const char* /*method*/, const char* path)
{
	UTF8Decode utf8(path + 5 /* /vod/ */);
	std::string fullpath = CWD;
	fullpath += utf8;
	printf("hls_server_onvod: %s\n", fullpath.c_str());

	if (path_testdir(fullpath.c_str()))
	{
		return http_list_dir(session, fullpath.c_str());
	}
	else if (path_testfile(fullpath.c_str()))
	{
		http_server_set_header(session, "Access-Control-Allow-Origin", "*");
		http_server_set_header(session, "Access-Control-Allow-Methods", "GET, POST, PUT");
		//http_server_set_header(session, "Transfer-Encoding", "chunked");
		if (std::string::npos != fullpath.find(".m3u8"))
			http_server_set_header(session, "content-type", HLS_M3U8_TYPE);
		else if (std::string::npos != fullpath.find(".mpd"))
			http_server_set_header(session, "content-type", "application/dash+xml");
		else if (std::string::npos != fullpath.find(".mp4") || std::string::npos != fullpath.find(".m4v"))
			http_server_set_header(session, "content-type", "video/mp4");
		else if (std::string::npos != fullpath.find(".m4a"))
			http_server_set_header(session, "content-type", "audio/mp4"); 
		return http_server_sendfile(session, fullpath.c_str(), NULL, NULL);
	}

	return http_server_send(session, 404, "", 0, NULL, NULL);
}

void hls_server_test(const char* ip, int port)
{
	aio_worker_init(4);
	http_server_t* http = http_server_create(ip, port);
	http_server_set_handler(http, http_server_route, http);
	http_server_addroute("/live/", hls_server_onlive);
	http_server_addroute("/vod/", hls_server_onvod);

	// http process
	while('q' != getchar())
	{
	}

	http_server_destroy(http);
	aio_worker_clean(4);
}

#if defined(_HLS_SERVER_TEST_)
int main(int argc, char* argv[])
{
	hls_server_test(NULL, 80);
	return 0;
}
#endif
