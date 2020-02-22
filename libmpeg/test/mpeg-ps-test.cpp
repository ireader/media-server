#include "mpeg-ps.h"
#include "mpeg-ts.h"
#include "mpeg-ts-proto.h"
#include <assert.h>
#include <stdio.h>
#include <map>
#include <string.h>

static void* ps_alloc(void* /*param*/, size_t bytes)
{
    static char s_buffer[2 * 1024 * 1024];
    assert(bytes <= sizeof(s_buffer));
    return s_buffer;
}

static void ps_free(void* /*param*/, void* /*packet*/)
{
    return;
}

static void ps_write(void* param, int stream, void* packet, size_t bytes)
{
    fwrite(packet, bytes, 1, (FILE*)param);
}

static inline const char* ps_type(int type)
{
    switch (type)
    {
    case PSI_STREAM_MP3: return "MP3";
    case PSI_STREAM_AAC: return "AAC";
    case PSI_STREAM_H264: return "H264";
    case PSI_STREAM_H265: return "H265";
    default: return "*";
    }
}

static void on_ts_packet(void* ps, int stream, int codecid, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes)
{
    printf("[%s] pts: %08lu, dts: %08lu%s\n", ps_type(codecid), (unsigned long)pts, (unsigned long)dts, flags ? " [I]" : "");

    int i;
    static std::map<int, int> streams;
    if (0 == streams.size())
    {
        i = ps_muxer_add_stream((ps_muxer_t*)ps, PSI_STREAM_AAC, NULL, 0);
        streams[PSI_STREAM_AAC] = i;
        i = ps_muxer_add_stream((ps_muxer_t*)ps, PSI_STREAM_H264, NULL, 0);
        streams[PSI_STREAM_H264] = i;
        //i = ps_muxer_add_stream((ps_muxer_t*)ps, PSI_STREAM_H265, NULL, 0);
        //streams[PSI_STREAM_H265] = i;
    }

    std::map<int, int>::const_iterator it = streams.find(codecid);
    if (streams.end() == it)
    {
        assert(0);
        i = ps_muxer_add_stream((ps_muxer_t*)ps, codecid, NULL, 0);
        streams[codecid] = i;
    }
    else
    {
        i = it->second;
    }

    ps_muxer_input((ps_muxer_t*)ps, i, flags, pts, dts, data, bytes);
}

static void ps_demuxer(const char* file, ps_muxer_t* muxer)
{
    static uint8_t packet[2 * 1024 * 1024];
    FILE* fp = fopen(file, "rb");

    size_t n, i = 0;
    ps_demuxer_t* ps = ps_demuxer_create(on_ts_packet, muxer);
    while ((n = fread(packet + i, 1, sizeof(packet) - i, fp)) > 0)
    {
        size_t r = ps_demuxer_input(ps, packet, n + i);
        memmove(packet, packet + r, n + i - r);
        i = n + i - r;
    }
    ps_demuxer_destroy(ps);

    fclose(fp);
}

//mpeg_ps_test("test/fileSequence0.ps", "test/fileSequence0.ps")
void mpeg_ps_test(const char* input)
{
    char output[256] = { 0 };
    snprintf(output, sizeof(output), "%s.ps", input);

    struct ps_muxer_func_t handler;
    handler.alloc = ps_alloc;
    handler.write = ps_write;
    handler.free = ps_free;

    FILE* fp = fopen(output, "wb");
    ps_muxer_t* ps = ps_muxer_create(&handler, fp);

    ps_demuxer(input, ps);

    ps_muxer_destroy(ps);
    fclose(fp);
}
