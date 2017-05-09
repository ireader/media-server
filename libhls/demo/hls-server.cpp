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
#include "urlcodec.h"
#include "time64.h"
#include "StdCFile.h"
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

	int i;
	std::list<hls_ts_t> files;
};

static std::map<std::string, hls_playlist_t*> s_playlists;

static void hls_handler(void* param, const void* data, size_t bytes, int64_t pts, int64_t /*dts*/, int64_t duration)
{
	hls_playlist_t* playlist = (hls_playlist_t*)param;

	char name[128] = { 0 };
	snprintf(name, sizeof(name), "%s/%d.ts", playlist->file.c_str(), playlist->i++);
	hls_m3u8_add(playlist->m3u8, name, pts, duration, 0);

	// add new segment
	hls_ts_t ts;
	ts.name = name;
	ts.size = bytes;
	ts.data = malloc(bytes);
	memcpy(ts.data, data, bytes);
	playlist->files.push_back(ts);

	// remove oldest segment
	while(playlist->files.size() > 4)
	{
		hls_ts_t ts = playlist->files.front();
		free(ts.data);
		playlist->files.pop_front();
	}

	printf("new segment: %s\n", name);
}

static void flv_handler(void* param, int type, const void* data, size_t bytes, uint32_t pts, uint32_t dts)
{
	static uint32_t s_dts = 0xFFFFFFFF;
	int discontinue = 0xFFFFFFFF != s_dts ? 0 : (dts > s_dts + HLS_DURATION / 2 ? 1 : 0);
	s_dts = dts;

	switch (type)
	{
	case FLV_AAC:
	case FLV_MP3:
		hls_media_input(param, FLV_AAC == type ? STREAM_AUDIO_AAC : STREAM_AUDIO_MP3, data, bytes, pts, dts, discontinue);
		break;

	case FLV_AVC:
		hls_media_input(param, STREAM_VIDEO_H264, data, bytes, pts, dts, discontinue);
		break;

	default:
		// nothing to do
		break;
	}
}

static int STDCALL hls_server_worker(void* param)
{
	int r, type;
	time64_t clock = 0;
	uint32_t timestamp = 0;
	hls_playlist_t* playlist = (hls_playlist_t*)param;

	std::string file = playlist->file + ".flv";
	void* flv = flv_reader_create(file.c_str());
	void* demuxer = flv_demuxer_create(flv_handler, playlist->hls);

	static unsigned char packet[2 * 1024 * 1024];
	while ((r = flv_reader_read(flv, &type, &timestamp, packet, sizeof(packet))) > 0)
	{
		time64_t now = time64_now();
		if(0 == clock)
		{
			clock = now;
		}
		else
		{
			if(timestamp > now - clock)
				system_sleep(timestamp - (now - clock));
		}

		assert(0 == flv_demuxer_input(demuxer, type, packet, r, timestamp));
	}

	flv_demuxer_destroy(demuxer);
	flv_reader_destroy(flv);
	hls_media_destroy(playlist->hls);
	//hls_m3u8_destroy(playlist->m3u8);
	//s_playlists.erase();
	//delete playlist;
	return thread_destroy(playlist->t);
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
	size_t n;
	char uri[256];
	const char* p;

	// decode request uri
	std::vector<std::string> paths;
	url_decode(path, -1, uri, sizeof(uri));
	printf("load uri: %s\n", uri);
	Split(uri, "/", paths);
	std::vector<std::string> names, param;
	Split(paths.rbegin()->c_str(), "?", param);
	assert(param.size() <= 2);
	Split(param.begin()->c_str(), ".", names);
	assert(2 == names.size());

	n = strlen(uri);
	if (0 == strncmp(uri, "/live/", 6) && 3 <= paths.size())
	{
		if (names[1] == "m3u8")
		{
			return hls_server_m3u8(session, names[0]);
		}
		else if (names[1] ==  "ts")
		{
			assert(4 == paths.size());
			return hls_server_ts(session, paths[2], paths[3]);
		}
		else if(names[1] == "flv")
		{
			hls_playlist_t* playlist = new hls_playlist_t();
			playlist->file = names[0];
			playlist->m3u8 = hls_m3u8_create(HLS_LIVE_NUM);
			playlist->hls = hls_media_create(HLS_DURATION * 1000, hls_handler, playlist);
			playlist->i = 0;
			s_playlists[names[0]] = playlist;

			thread_create(&playlist->t, hls_server_worker, playlist);
		}
	}
	else if (names[1] == "xml")
	{
		StdCFile f(std::string(names[0] + "." + names[1]).c_str(), "r");
		std::auto_ptr<char> p((char*)f.Read());
		return hls_server_reply(session, 200, p.get());
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
