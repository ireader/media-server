#include "mkv-format.h"
#include "mkv-reader.h"
#include "mkv-writer.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

extern "C" const struct mkv_buffer_t* mkv_file_buffer(void);

static uint8_t s_buffer[2 * 1024 * 1024];
static int s_audio_track = -1;
static int s_video_track = -1;
static int s_subtitle_track = -1;

static void mkv_onread(void* param, uint32_t track, const void* buffer, size_t bytes, int64_t pts, int64_t dts, int flags)
{
	mkv_writer_t* mkv = (mkv_writer_t*)param;
    int r = mkv_writer_write(mkv, track, buffer, bytes, pts, dts, flags);
    assert(0 == r);
}

static void mkv_video_info(void* param, uint32_t track, enum mkv_codec_t codec, int width, int height, const void* extra, size_t bytes)
{
    mkv_writer_t* mkv = (mkv_writer_t*)param;
    s_video_track = mkv_writer_add_video(mkv, codec, width, height, extra, bytes);
}

static void mkv_audio_info(void* param, uint32_t track, enum mkv_codec_t codec, int channel_count, int bit_per_sample, int sample_rate, const void* extra, size_t bytes)
{
    mkv_writer_t* mkv = (mkv_writer_t*)param;
    s_audio_track = mkv_writer_add_audio(mkv, codec, channel_count, bit_per_sample, sample_rate, extra, bytes);
}

static void mkv_subtitle_info(void* param, uint32_t track, enum mkv_codec_t codec, const void* extra, size_t bytes)
{
    mkv_writer_t* mkv = (mkv_writer_t*)param;
    s_subtitle_track = mkv_writer_add_subtitle(mkv, codec, extra, bytes);
}

void mkv_writer_test2(const char* file, const char* outfile)
{
    FILE* fp = fopen(file, "rb");
    FILE* wfp = fopen(outfile, "wb");
    mkv_reader_t* mkv = mkv_reader_create(mkv_file_buffer(), fp);
    mkv_writer_t* w = mkv_writer_create(mkv_file_buffer(), wfp, 0);

    struct mkv_reader_trackinfo_t info = { mkv_video_info, mkv_audio_info, mkv_subtitle_info };
    mkv_reader_getinfo(mkv, &info, w);
    
    while (mkv_reader_read(mkv, s_buffer, sizeof(s_buffer), mkv_onread, w) > 0)
    {
    }

    mkv_writer_destroy(w);
    mkv_reader_destroy(mkv);
    fclose(fp);
    fclose(wfp);
}
