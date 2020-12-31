#include "mov-writer.h"
#include "mov-format.h"
#include "mov-reader.h"
#include "mpeg4-aac.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

extern "C" const struct mov_buffer_t* mov_file_buffer(void);

static uint8_t s_buffer[2 * 1024 * 1024];
static int s_audio_track = -1;
static int s_video_track = -1;
static int s_pts_last = 0;

static void mov_onread(void* param, uint32_t track, const void* buffer, size_t bytes, int64_t pts, int64_t dts, int flags)
{
    s_pts_last = pts;
    mov_writer_t* mov = (mov_writer_t*)param;
    int r = mov_writer_write(mov, track - 1, buffer, bytes, pts, dts, flags);
    assert(0 == r);
}

static void mov_video_info(void* param, uint32_t track, uint8_t object, int width, int height, const void* extra, size_t bytes)
{
    mov_writer_t* mov = (mov_writer_t*)param;
    s_video_track = mov_writer_add_video(mov, object, width, height, extra, bytes);
}

static void mov_audio_info(void* param, uint32_t track, uint8_t object, int channel_count, int bit_per_sample, int sample_rate, const void* extra, size_t bytes)
{
    mov_writer_t* mov = (mov_writer_t*)param;
    s_audio_track = mov_writer_add_audio(mov, object, channel_count, bit_per_sample, sample_rate, extra, bytes);
}

void mov_writer_subtitle(const char* mp4, const char* outmp4)
{
    char sbuf[128];
    static const char* s_subtitles[] = {
        "line 1",
        "message 1",
        "line 2",
        "message 2",
    };

    FILE* rfp = fopen(mp4, "rb");
    FILE* wfp = fopen(outmp4, "wb");
    mov_reader_t* rmov = mov_reader_create(mov_file_buffer(), rfp);
    mov_writer_t* wmov = mov_writer_create(mov_file_buffer(), wfp, 0);

    struct mov_reader_trackinfo_t info = { mov_video_info, mov_audio_info };
    mov_reader_getinfo(rmov, &info, wmov);
    
    int i = 0;
    int track = mov_writer_add_subtitle(wmov, MOV_OBJECT_TEXT, NULL, 0);

    while (mov_reader_read(rmov, s_buffer, sizeof(s_buffer), mov_onread, wmov) > 0)
    {
        if (0 == (++i % 100))
        {
            const char* t = s_subtitles[(i / 100) % (sizeof(s_subtitles)/sizeof(s_subtitles[0]))];
            assert(strlen(t) < 0xFFFF);
            size_t n = strlen(t);
            sbuf[0] = (n >> 8) & 0xFF;
            sbuf[1] = n & 0xFF;
            memcpy(sbuf + 2, t, n);
            mov_writer_write(wmov, track, sbuf, n+2, s_pts_last, s_pts_last, 0);
        }
    }

    mov_writer_destroy(wmov);
    mov_reader_destroy(rmov);
    fclose(rfp);
    fclose(wfp);
}
