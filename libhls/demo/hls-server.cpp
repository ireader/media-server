#include "aio-socket.h"
#include "mpeg-ps.h"
#include "mpeg-ts.h"
#include "hls-m3u8.h"
#include "hls-media.h"
#include "hls-param.h"
#include "flv-reader.h"
#include "flv-demuxer.h"
#include "http-server.h"
#include "sys/thread.h"
#include "sys/system.h"
#include "sys/path.h"
#include "urlcodec.h"
#include "url.h"
#include "time64.h"
#include "StdCFile.h"
#include "cstringext.h"
#include "cppstringext.h"
#include <string.h>
#include <assert.h>
#include <map>
#include <list>
#include <string>

struct hls_ts_t
{
	void* data;
	size_t size;
	std::string name;
};

struct hls_playlist_t
{
	pthread_t t;
	std::string file;

	void* hls;
	void* m3u8;
	int64_t pts;
	int64_t last_pts;

	int i;
	std::list<hls_ts_t> files;
};

static std::map<std::string, hls_playlist_t*> s_playlists;

static void hls_handler(void* param, const void* data, size_t bytes, int64_t pts, int64_t /*dts*/, int64_t duration)
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
	hls_ts_t ts;
	ts.name = name;
	ts.size = bytes;
	ts.data = malloc(bytes);
	memcpy(ts.data, data, bytes);
	playlist->files.push_back(ts);

	// remove oldest segment
	while(playlist->files.size() > HLS_LIVE_NUM + 1)
	{
		hls_ts_t ts = playlist->files.front();
		free(ts.data);
		playlist->files.pop_front();
	}

	printf("new segment: %s\n", name);
}

static void flv_handler(void* param, int type, const void* data, size_t bytes, uint32_t pts, uint32_t dts)
{
	switch (type)
	{
	case FLV_AAC:
	case FLV_MP3:
		hls_media_input(param, FLV_AAC == type ? STREAM_AUDIO_AAC : STREAM_AUDIO_MP3, data, bytes, pts, dts, 0);
		break;

	case FLV_AVC:
		hls_media_input(param, STREAM_VIDEO_H264, data, bytes, pts, dts, 0);
		break;

	default:
		// nothing to do
		break;
	}
}

static int STDCALL hls_server_worker(void* param)
{
	int r, type;
	time64_t clock;
	uint32_t timestamp;
	hls_playlist_t* playlist = (hls_playlist_t*)param;
	std::string file = playlist->file + ".flv";

	while (1)
	{
		void* flv = flv_reader_create(file.c_str());
		void* demuxer = flv_demuxer_create(flv_handler, playlist->hls);

		clock = 0;
		static unsigned char packet[2 * 1024 * 1024];
		while ((r = flv_reader_read(flv, &type, &timestamp, packet, sizeof(packet))) > 0)
		{
			time64_t now = time64_now();
			if (0 == clock)
			{
				clock = now;
			}
			else
			{
				if (timestamp > now - clock)
					system_sleep(timestamp - (now - clock));
			}

			assert(0 == flv_demuxer_input(demuxer, type, packet, r, timestamp));
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

static int hls_server_reply_file(void* session, const char* file)
{
	static char buffer[4 * 1024 * 1024];
	StdCFile f(file, "rb");
	int r = f.Read(buffer, sizeof(buffer));

	void* ptr;
	void* bundle;
	bundle = http_bundle_alloc(r);
	ptr = http_bundle_lock(bundle);
	memcpy(ptr, buffer, r);
	http_bundle_unlock(bundle, r);
	http_server_set_header(session, "Access-Control-Allow-Origin", "*");
	http_server_set_header(session, "Access-Control-Allow-Methods", "GET, POST, PUT");
	http_server_send(session, 200, bundle);
	http_bundle_free(bundle);
	return 0;
}

static int hls_server_reply(void* session, int code, const char* msg)
{
	void* ptr;
	void* bundle;
	bundle = http_bundle_alloc(strlen(msg) + 1);
	ptr = http_bundle_lock(bundle);
	strcpy((char*)ptr, msg);
	http_bundle_unlock(bundle, strlen(msg) + 1);
	http_server_set_header(session, "Access-Control-Allow-Origin", "*");
	http_server_set_header(session, "Access-Control-Allow-Methods", "GET, POST, PUT");
	http_server_send(session, code, bundle);
	http_bundle_free(bundle);
	return 0;
}

static int hls_server_m3u8(void* session, const std::string& path)
{
	void* m3u8 = s_playlists.find(path)->second->m3u8;
	assert(m3u8);

	void* bundle = http_bundle_alloc(4 * 1024);
	void* ptr = http_bundle_lock(bundle);
	assert(0 == hls_m3u8_playlist(m3u8, 0, (char*)ptr, 4 * 1024));
	http_bundle_unlock(bundle, strlen((char*)ptr));

	http_server_set_header(session, "content-type", HLS_M3U8_TYPE);
	http_server_set_header(session, "Access-Control-Allow-Origin", "*");
	http_server_set_header(session, "Access-Control-Allow-Methods", "GET, POST, PUT");
	http_server_send(session, 200, bundle);
	http_bundle_free(bundle);

	printf("load %s.m3u8 file\n", path.c_str());
	return 0;
}

static int hls_server_ts(void* session, const std::string& path, const std::string& ts)
{
	hls_playlist_t* playlist = s_playlists.find(path)->second;
	assert(playlist);

	std::string file = path + '/' + ts;
	for(auto i = playlist->files.begin(); i != playlist->files.end(); ++i)
	{
		if(i->name == file)
		{
			void* bundle = http_bundle_alloc(i->size);
			void* ptr = http_bundle_lock(bundle);
			memcpy(ptr, i->data, i->size);
			http_bundle_unlock(bundle, i->size);

			http_server_set_header(session, "Access-Control-Allow-Origin", "*");
			http_server_set_header(session, "Access-Control-Allow-Methods", "GET, POST, PUT");
			http_server_send(session, 200, bundle);
			http_bundle_free(bundle);

			printf("load file %s\n", file.c_str());
			return 0;
		}
	}

	printf("load ts file(%s) failed\n", file.c_str());
	return hls_server_reply(session, 404, "");
}

static int hls_server_onhttp(void* http, void* session, const char* method, const char* path)
{
	// decode request uri
	void* url = url_parse(path);
	std::string s = url_getpath(url);
	url_free(url);
	path = s.c_str();

	if (0 == strncmp(path, "/live/", 6))
	{
		std::vector<std::string> paths;
		Split(path + 6, "/", paths);

		if (strendswith(path, ".m3u8") && 1 == paths.size())
		{
			std::string app = paths[0].substr(0, paths[0].length() - 5);
			if (s_playlists.find(app) == s_playlists.end())
			{
				hls_playlist_t* playlist = new hls_playlist_t();
				playlist->file = app;
				playlist->m3u8 = hls_m3u8_create(HLS_LIVE_NUM);
				playlist->hls = hls_media_create(HLS_DURATION * 1000, hls_handler, playlist);
				playlist->i = 0;
				s_playlists[app] = playlist;

				thread_create(&playlist->t, hls_server_worker, playlist);
			}

			return hls_server_m3u8(session, app);
		}
		else if (strendswith(path, ".ts") && 2 == paths.size())
		{
			if (s_playlists.find(paths[0]) != s_playlists.end())
			{
				return hls_server_ts(session, paths[0], paths[1]);
			}
		}
	}
	else if (0 == strncmp(path, "/vod/", 5))
	{
		if (path_testfile(path+5))
		{
			return hls_server_reply_file(session, path + 5);
		}
	}

	return hls_server_reply(session, 404, "");
}

void hls_server_test(const char* ip, int port)
{
	aio_socket_init(1);
	http_server_init();
	void* http = http_server_create(ip, port);
	http_server_set_handler(http, hls_server_onhttp, http);

	// http process
	while(aio_socket_process(1000) >= 0)
	{
	}

	http_server_destroy(http);
	http_server_cleanup();
	aio_socket_clean();
}
