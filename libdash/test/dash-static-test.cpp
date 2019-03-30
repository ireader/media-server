#include "dash-mpd.h"
#include "dash-proto.h"
#include "mov-format.h"
#include "mov-reader.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

extern "C" const struct mov_buffer_t* mov_file_buffer(void);

static char s_packet[2 * 1024 * 1024];
static uint32_t s_track_video;
static uint32_t s_track_audio;
static int s_adapation_video;
static int s_adapation_audio;

static void mp4_onvideo(void* mpd, uint32_t track, uint8_t object, int width, int height, const void* extra, size_t bytes)
{
	s_track_video = track;
	s_adapation_video = dash_mpd_add_video_adaptation_set((dash_mpd_t*)mpd, "dash-static-video", object, width, height, extra, bytes);
}

static void mp4_onaudio(void* mpd, uint32_t track, uint8_t object, int channel_count, int bit_per_sample, int sample_rate, const void* extra, size_t bytes)
{
	s_track_audio = track;
	s_adapation_audio = dash_mpd_add_audio_adaptation_set((dash_mpd_t*)mpd, "dash-static-audio", object, channel_count, bit_per_sample, sample_rate, extra, bytes);
}

static void mp4_onread(void* mpd, uint32_t track, const void* buffer, size_t bytes, int64_t pts, int64_t dts, int flags)
{
	if (s_track_video == track)
	{
		bool keyframe = 5 == (0x1f & ((uint8_t*)buffer)[4]);
		dash_mpd_input((dash_mpd_t*)mpd, s_adapation_video, buffer, bytes, pts, dts, keyframe ? MOV_AV_FLAG_KEYFREAME : 0);
	}
	else if (s_track_audio == track)
	{
		dash_mpd_input((dash_mpd_t*)mpd, s_adapation_audio, buffer, bytes, pts, dts, 0);
	}
	else
	{
		assert(0);
	}
}

static int dash_mpd_onsegment(void* /*param*/, int /*track*/, const void* data, size_t bytes, int64_t /*pts*/, int64_t /*dts*/, int64_t /*duration*/, const char* name)
{
	FILE* fp = fopen(name, "wb");
	fwrite(data, 1, bytes, fp);
	fclose(fp);
	return 0;
}

static void dash_save_playlist(const char* name, const char* playlist)
{
	char filename[256];
	snprintf(filename, sizeof(filename), "%s.mpd", name);
	FILE* fp = fopen(filename, "wb");
	fwrite(playlist, 1, strlen(playlist), fp);
	fclose(fp);
}

void dash_static_test(const char* mp4, const char* name)
{
	FILE* fp = fopen(mp4, "rb");
	mov_reader_t* mov = mov_reader_create(mov_file_buffer(), fp);
	dash_mpd_t* mpd = dash_mpd_create(DASH_STATIC, dash_mpd_onsegment, NULL);

	struct mov_reader_trackinfo_t info = { mp4_onvideo, mp4_onaudio };
	mov_reader_getinfo(mov, &info, mpd);
	int r = mov_reader_read(mov, s_packet, sizeof(s_packet), mp4_onread, mpd);
	while (1 == r)
	{
		r = mov_reader_read(mov, s_packet, sizeof(s_packet), mp4_onread, mpd);
	}

	//flush
	dash_mpd_input(mpd, s_adapation_video, NULL, 0, 0, 0, 0);
	dash_mpd_playlist(mpd, s_packet, sizeof(s_packet));
	dash_save_playlist(name, s_packet);

	dash_mpd_destroy(mpd);
	mov_reader_destroy(mov);
	fclose(fp);
}
