#include "webm-reader.h"
#include "mpeg4-hevc.h"
#include "mpeg4-avc.h"
#include "mpeg4-aac.h"
#include "opus-head.h"
#include "webm-vpx.h"
#include "aom-av1.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static uint8_t s_packet[2 * 1024 * 1024];
static uint8_t s_buffer[4 * 1024 * 1024];
static FILE* s_vfp, * s_afp;
static struct mpeg4_hevc_t s_hevc;
static struct mpeg4_avc_t s_avc;
static struct mpeg4_aac_t s_aac;
static struct webm_vpx_t s_vpx;
static struct opus_head_t s_opus;
static struct aom_av1_t s_av1;
static uint32_t s_aac_track = 0xFFFFFFFF;
static uint32_t s_avc_track = 0xFFFFFFFF;
static uint32_t s_av1_track = 0xFFFFFFFF;
static uint32_t s_vpx_track = 0xFFFFFFFF;
static uint32_t s_hevc_track = 0xFFFFFFFF;
static uint32_t s_opus_track = 0xFFFFFFFF;
static uint32_t s_mp3_track = 0xFFFFFFFF;
static uint32_t s_subtitle_track = 0xFFFFFFFF;

extern "C" const struct webm_buffer_t* webm_file_buffer(void);

inline const char* ftimestamp(uint32_t t, char* buf)
{
	sprintf(buf, "%02u:%02u:%02u.%03u", t / 3600000, (t / 60000) % 60, (t / 1000) % 60, t % 1000);
	return buf;
}

static void webm_reader_test_onread(void* flv, uint32_t track, const void* buffer, size_t bytes, int64_t pts, int64_t dts, int flags)
{
	static char s_pts[64], s_dts[64];
	static int64_t v_pts, v_dts;
	static int64_t a_pts, a_dts;
}

static void webm_video_info(void* /*param*/, uint32_t track, uint8_t object, int /*width*/, int /*height*/, const void* extra, size_t bytes)
{
}

static void webm_audio_info(void* /*param*/, uint32_t track, uint8_t object, int channel_count, int /*bit_per_sample*/, int sample_rate, const void* extra, size_t bytes)
{
}

static void webm_subtitle_info(void* /*param*/, uint32_t track, uint8_t object, const void* /*extra*/, size_t /*bytes*/)
{
	s_subtitle_track = track;
}

void webm_reader_test(const char* file)
{
	FILE* fp = fopen(file, "rb");
	webm_reader_t* mov = webm_reader_create(webm_file_buffer(), fp);
	uint64_t duration = webm_reader_getduration(mov);

	struct webm_reader_trackinfo_t info = { webm_video_info, webm_audio_info, webm_subtitle_info };
	webm_reader_getinfo(mov, &info, NULL);

	while (webm_reader_read(mov, s_buffer, sizeof(s_buffer), webm_reader_test_onread, NULL) > 0)
	{
	}

	duration /= 2;
	webm_reader_seek(mov, (int64_t*)&duration);

	webm_reader_destroy(mov);
	if (s_vfp) fclose(s_vfp);
	if (s_afp) fclose(s_afp);
	fclose(fp);
}
