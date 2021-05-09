#include "fmp4-writer.h"
#include "mov-format.h"
#include "mov-reader.h"
#include "mpeg4-aac.h"
#include "mov-file-buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static uint8_t s_buffer[2 * 1024 * 1024];
static int s_audio_track = -1;
static int s_video_track = -1;

static void mov_onread(void* param, uint32_t track, const void* buffer, size_t bytes, int64_t pts, int64_t dts, int flags)
{
	fmp4_writer_t* fmp4 = (fmp4_writer_t*)param;
    int r = fmp4_writer_write(fmp4, track-1, buffer, bytes, pts, dts, flags);
    assert(0 == r);
}

static void mov_video_info(void* param, uint32_t track, uint8_t object, int width, int height, const void* extra, size_t bytes)
{
    fmp4_writer_t* fmp4 = (fmp4_writer_t*)param;
    s_video_track = fmp4_writer_add_video(fmp4, object, width, height, extra, bytes);
}

static void mov_audio_info(void* param, uint32_t track, uint8_t object, int channel_count, int bit_per_sample, int sample_rate, const void* extra, size_t bytes)
{
    fmp4_writer_t* fmp4 = (fmp4_writer_t*)param;
    s_audio_track = fmp4_writer_add_audio(fmp4, object, channel_count, bit_per_sample, sample_rate, extra, bytes);
}

void fmp4_writer_test2(const char* mp4, const char* outmp4)
{
    struct mov_file_cache_t file, wfile;
    memset(&file, 0, sizeof(file));
    memset(&wfile, 0, sizeof(wfile));
    file.fp = fopen(mp4, "rb");
    wfile.fp = fopen(outmp4, "wb");
    mov_reader_t* mov = mov_reader_create(mov_file_cache_buffer(), &file);
    fmp4_writer_t* fmp4 = fmp4_writer_create(mov_file_cache_buffer(), &wfile, MOV_FLAG_SEGMENT);

    struct mov_reader_trackinfo_t info = { mov_video_info, mov_audio_info };
    mov_reader_getinfo(mov, &info, fmp4);
    fmp4_writer_init_segment(fmp4);
    
    while (mov_reader_read(mov, s_buffer, sizeof(s_buffer), mov_onread, fmp4) > 0)
    {
    }

    fmp4_writer_destroy(fmp4);
    mov_reader_destroy(mov);
    fclose(wfile.fp);
    fclose(wfile.fp);
}
