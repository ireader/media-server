#include "aio-socket.h"
#include "aio-timeout.h"
#include "dash-mpd.h"
#include "dash-proto.h"
#include "mov-reader.h"
#include "mov-format.h"
#include "http-server.h"
#include "http-route.h"
#include "cstringext.h"
#include "sys/thread.h"
#include "sys/system.h"
#include "sys/path.h"
#include <string>
#include <vector>
#include <map>

struct dash_playlist_t
{
	std::string name;
	dash_mpd_t* mpd;

	int64_t timestamp;
	int adapation_video;
	int adapation_audio;
	uint32_t track_video;
	uint32_t track_audio;
	char playlist[256 * 1024];
	uint8_t packet[2 * 1024 * 1024];
};

std::map<std::string, dash_playlist_t*> s_playlists;

extern "C" const struct mov_buffer_t* mov_file_buffer(void);

static void mp4_onvideo(void* param, uint32_t track, uint8_t object, int width, int height, const void* extra, size_t bytes)
{
	dash_playlist_t* dash = (dash_playlist_t*)param;
	dash->track_video = track;
	dash->adapation_video = dash_mpd_add_video_adapation_set(dash->mpd, dash->name.c_str(), object, width, height, extra, bytes);
}

static void mp4_onaudio(void* param, uint32_t track, uint8_t object, int channel_count, int bit_per_sample, int sample_rate, const void* extra, size_t bytes)
{
	dash_playlist_t* dash = (dash_playlist_t*)param;
	dash->track_audio = track;
	dash->adapation_audio = dash_mpd_add_audio_adapation_set(dash->mpd, dash->name.c_str(), object, channel_count, bit_per_sample, sample_rate, extra, bytes);
}

static void mp4_onread(void* param, uint32_t track, const void* buffer, size_t bytes, int64_t pts, int64_t dts)
{
	dash_playlist_t* dash = (dash_playlist_t*)param;
	dash->timestamp = dts;

	if (dash->track_video == track)
	{
		bool keyframe = 5 == (0x1f & ((uint8_t*)buffer)[4]);
		dash_mpd_input(dash->mpd, dash->adapation_video, buffer, bytes, pts, dts, keyframe ? MOV_AV_FLAG_KEYFREAME : 0);
	}
	else if (dash->track_audio == track)
	{
		dash_mpd_input(dash->mpd, dash->adapation_audio, buffer, bytes, pts, dts, 0);
	}
	else
	{
		assert(0);
	}
}

static int dash_mpd_onsegment(void* param, int /*track*/, const void* data, size_t bytes, int64_t /*pts*/, int64_t /*dts*/, int64_t /*duration*/, const char* name)
{
	FILE* fp = fopen(name, "wb");
	fwrite(data, 1, bytes, fp);
	fclose(fp);

	dash_playlist_t* dash = (dash_playlist_t*)param;
	dash_mpd_playlist(dash->mpd, dash->playlist, sizeof(dash->playlist));
	return 0;
}

static int STDCALL dash_server_worker(void* param)
{
	uint64_t clock = 0;
	dash_playlist_t* dash = (dash_playlist_t*)param;
	std::string file = dash->name.substr(0, dash->name.find('.', 0)) + ".mp4";

	FILE* fp = fopen(file.c_str(), "rb");
	mov_reader_t* mov = mov_reader_create(mov_file_buffer(), fp);
	struct mov_reader_trackinfo_t info = { mp4_onvideo, mp4_onaudio };
	mov_reader_getinfo(mov, &info, dash);
	int r = mov_reader_read(mov, dash->packet, sizeof(dash->packet), mp4_onread, dash);

	while (1 == r)
	{
		uint64_t now = system_clock();
		if (0 == clock)
		{
			clock = now - 30000;
		}
		else
		{
			if (dash->timestamp > (int64_t)(now - clock))
				system_sleep(dash->timestamp - (int64_t)(now - clock));
		}

		printf("timestamp: %lld, now: %llu\n", dash->timestamp, now);
		r = mov_reader_read(mov, dash->packet, sizeof(dash->packet), mp4_onread, dash);
	}

	mov_reader_destroy(mov);
	fclose(fp);
	//dash_mpd_destroy(dash->mpd);
	//s_playlists.erase();
	//delete playlist;
	return 0;
}

static int dash_server_mpd(http_session_t* session, const char* name)
{
	dash_playlist_t* dash = s_playlists.find(name)->second;
	http_server_set_header(session, "Content-Type", "application/xml+dash");
	http_server_set_header(session, "Access-Control-Allow-Origin", "*");
	http_server_set_header(session, "Access-Control-Allow-Methods", "GET, POST, PUT");
	http_server_reply(session, 200, dash->playlist, strlen(dash->playlist));
	return 0;
}

static int dash_server_onlive(void* /*http*/, http_session_t* session, const char* /*method*/, const char* path)
{
	printf("live: %s\n", path);
	const char* name = path_basename(path);
	if (strendswith(name, ".mpd"))
	{
		if (s_playlists.find(name) == s_playlists.end())
		{
			dash_playlist_t* dash = new dash_playlist_t();
			dash->name = name;
			dash->mpd = dash_mpd_create(DASH_DYNAMIC, dash_mpd_onsegment, dash);
			dash_mpd_playlist(dash->mpd, dash->playlist, sizeof(dash->playlist));
			s_playlists[name] = dash;

			pthread_t thread;
			thread_create(&thread, dash_server_worker, dash);
			thread_detach(thread);
		}

		return dash_server_mpd(session, name);
	}
	else if (path_testfile(name))
	{
		// cross domian
		http_server_set_header(session, "Access-Control-Allow-Origin", "*");
		http_server_set_header(session, "Access-Control-Allow-Methods", "GET, POST, PUT");
		return http_server_sendfile(session, name, NULL, NULL);
	}

	return http_server_send(session, 404, "", 0, NULL, NULL);
}

static int dash_server_onvod(void* /*http*/, http_session_t* session, const char* /*method*/, const char* path)
{
	std::string fullpath = ".\\";
	fullpath += path + 5 /* /vod/ */;

	printf("vod: %s\n", fullpath.c_str());
	if (path_testfile(fullpath.c_str()))
	{
		// MIME
		if (strendswith(fullpath.c_str(), ".mpd"))
			http_server_set_content_type(session, "application/xml+dash");
		else
			http_server_set_content_type(session, "video/mp4");

		//http_server_set_header(session, "Transfer-Encoding", "chunked");

		// cross domian
		http_server_set_header(session, "Access-Control-Allow-Origin", "*");
		http_server_set_header(session, "Access-Control-Allow-Methods", "GET, POST, PUT");

		return http_server_sendfile(session, fullpath.c_str(), NULL, NULL);
	}

	return http_server_send(session, 404, "", 0, NULL, NULL);
}

void dash_dynamic_test(const char* ip, int port)
{
	aio_socket_init(1);
	http_server_t* http = http_server_create(ip, port);
	http_server_set_handler(http, http_server_route, http);
	http_server_addroute("/live/", dash_server_onlive);
	http_server_addroute("/vod/", dash_server_onvod);

	// http process
	while (aio_socket_process(10000) >= 0)
	{
		aio_timeout_process();
	}

	http_server_destroy(http);
	aio_socket_clean();
}
