#include "mkv-reader.h"
#include "mov-format.h"
#include "mov-writer.h"
#include "webm-vpx.h"
#include "rtsp-payloads.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static uint8_t s_buffer[4 * 1024 * 1024];
static int s_audio_track;
static int s_video_track;
static int s_subtitle_track;

extern "C" const struct mkv_buffer_t* mkv_file_buffer(void);
extern "C" const struct mov_buffer_t* mov_file_buffer(void);

static inline const char* ftimestamp(int64_t timestamp, char* buf)
{
	uint32_t t = (uint32_t)timestamp;
	sprintf(buf, "%02u:%02u:%02u.%03u", t / 3600000, (t / 60000) % 60, (t / 1000) % 60, t % 1000);
	return buf;
}

static void mkv_reader_test_onread(void* mov, uint32_t track, const void* buffer, size_t bytes, int64_t pts, int64_t dts, int flags)
{
	static char s_pts[64], s_dts[64];
	
	if (s_video_track == track)
	{
		static int64_t v_pts, v_dts;
		printf("[V] pts: %s, dts: %s, diff: %03d/%03d, bytes: %u%s\n", ftimestamp(pts, s_pts), ftimestamp(dts, s_dts), (int)(pts - v_pts), (int)(dts - v_dts), (unsigned int)bytes, flags ? " [I]" : "");
		v_pts = pts;
		v_dts = dts;
		mov_writer_write((mov_writer_t*)mov, s_video_track-1, buffer, bytes, pts, dts, flags & MKV_FLAGS_KEYFRAME ? MOV_AV_FLAG_KEYFREAME : 0);
	}
	else if (s_audio_track == track)
	{
		static int64_t a_pts, a_dts;
		printf("[A] pts: %s, dts: %s, diff: %03d/%03d, bytes: %u\n", ftimestamp(pts, s_pts), ftimestamp(dts, s_dts), (int)(pts - a_pts), (int)(dts - a_dts), (unsigned int)bytes);
		a_pts = pts;
		a_dts = dts;
		mov_writer_write((mov_writer_t*)mov, s_audio_track-1, buffer, bytes, pts, dts, 0);
	}
	else if (s_subtitle_track == track)
	{
		static int64_t t_pts, t_dts;
		printf("[S] pts: %s, dts: %s, diff: %03d/%03d, bytes: %u, text: %.*s\n", ftimestamp(pts, s_pts), ftimestamp(dts, s_dts), (int)(pts - t_pts), (int)(dts - t_dts), (unsigned int)bytes, (int)bytes - 2, (const char*)buffer + 2);
		t_pts = pts;
		t_dts = dts;
	}
	else
	{
		static int64_t x_pts, x_dts;
		printf("[%d] pts: %s, dts: %s, diff: %03d/%03d, bytes: %u%s\n", track, ftimestamp(pts, s_pts), ftimestamp(dts, s_dts), (int)(pts - x_pts), (int)(dts - x_dts), (unsigned int)bytes, flags ? " [I]" : "");
		x_pts = pts;
		x_dts = dts;
		//assert(0);
	}
}

static uint8_t mkv_codec_id_to_mov_object_id(enum mkv_codec_t codec)
{
	int i = avpayload_find_by_mkv(codec);
	if (-1 == i)
		return 0;
	return s_payloads[i].mov;
}

static void mkv_video_info(void* mov, uint32_t track, enum mkv_codec_t codec, int width, int height, const void* extra, size_t bytes)
{
	// TODO:
	if (bytes < 1 && (MKV_CODEC_VIDEO_VP8 == codec || MKV_CODEC_VIDEO_VP9 == codec))
	{
		int w, h;
		uint8_t buffer[128];
		struct webm_vpx_t vpx;
		memset(&vpx, 0, sizeof(vpx));
		webm_vpx_codec_configuration_record_from_vp9(&vpx, &w, &h, NULL, 0);
		bytes = webm_vpx_codec_configuration_record_save(&vpx, buffer, sizeof(buffer));
		extra = buffer; // override
	}

	s_video_track = track;
	int t = mov_writer_add_video((mov_writer_t*)mov, mkv_codec_id_to_mov_object_id(codec), width, height, extra, bytes);
	assert(t == s_video_track-1);
}

static void mkv_audio_info(void* mov, uint32_t track, enum mkv_codec_t codec, int channel_count, int bit_per_sample, int sample_rate, const void* extra, size_t bytes)
{
	s_audio_track = track;
	int t = mov_writer_add_audio((mov_writer_t*)mov, mkv_codec_id_to_mov_object_id(codec), channel_count, bit_per_sample, sample_rate, extra, bytes);
	assert(t == s_audio_track-1);
}

static void mkv_subtitle_info(void* /*param*/, uint32_t track, enum mkv_codec_t codec, const void* /*extra*/, size_t /*bytes*/)
{
	s_subtitle_track = track;
}

void mkv_2_mp4_test(const char* src, const char* mp4)
{
	FILE* fp = fopen(src, "rb");
	mkv_reader_t* mkv = mkv_reader_create(mkv_file_buffer(), fp);
	uint64_t duration = mkv_reader_getduration(mkv);

	FILE* wfp = fopen(mp4, "wb");
	mov_writer_t* mov = mov_writer_create(mov_file_buffer(), wfp, 0);

	struct mkv_reader_trackinfo_t info = { mkv_video_info, mkv_audio_info, mkv_subtitle_info };
	mkv_reader_getinfo(mkv, &info, mov);

	while (mkv_reader_read(mkv, s_buffer, sizeof(s_buffer), mkv_reader_test_onread, mov) > 0)
	{
	}

	duration /= 2;
	mkv_reader_seek(mkv, (int64_t*)&duration);

	mov_writer_destroy(mov);
	mkv_reader_destroy(mkv);
	fclose(fp);
	fclose(wfp);
}
