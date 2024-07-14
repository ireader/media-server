#include "mpeg-ps.h"
#include "mpeg-types.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <map>

static FILE* vfp;
static FILE* afp;

inline const char* ftimestamp(int64_t t, char* buf)
{
    if (PTS_NO_VALUE == t)
    {
        sprintf(buf, "(null)");
    }
    else
    {
        t /= 90;
        sprintf(buf, "%d:%02d:%02d.%03d", (int)(t / 3600000), (int)((t / 60000) % 60), (int)((t / 1000) % 60), (int)(t % 1000));
    }
    return buf;
}

static int onpacket(void* /*param*/, int stream, int avtype, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes)
{
    static std::map<int, std::pair<int64_t, int64_t>> s_streams;
    static char s_pts[64], s_dts[64];

    auto it = s_streams.find(stream);
    if (it == s_streams.end())
        it = s_streams.insert(std::make_pair(stream, std::pair<int64_t, int64_t>(pts, dts))).first;

    if (PTS_NO_VALUE == dts)
        dts = pts;

    if (PSI_STREAM_AAC == avtype || PSI_STREAM_AUDIO_G711A == avtype || PSI_STREAM_AUDIO_G711U == avtype)
    {
        //assert(0 == a_dts || dts >= a_dts);
        printf("[A] pts: %s(%" PRId64 "), dts: %s(%" PRId64 "), diff: %03d/%03d, size: %u\n", ftimestamp(pts, s_pts), pts, ftimestamp(dts, s_dts), dts, (int)(pts - it->second.first) / 90, (int)(dts - it->second.second) / 90, (unsigned int)bytes);
        fwrite(data, 1, bytes, afp);
    }
    else if (PSI_STREAM_H264 == avtype || PSI_STREAM_H265 == avtype || PSI_STREAM_VIDEO_SVAC == avtype)
    {
        //assert(0 == v_dts || dts >= v_dts);
        printf("[V] pts: %s(%" PRId64 "), dts: %s(%" PRId64 "), diff: %03d/%03d, size: %u%s\n", ftimestamp(pts, s_pts), pts, ftimestamp(dts, s_dts), dts, (int)(pts - it->second.first) / 90, (int)(dts - it->second.second) / 90, (unsigned int)bytes, (flags & MPEG_FLAG_IDR_FRAME) ? " [I]": "");
        fwrite(data, 1, bytes, vfp);
    }
    else
    {
        //assert(0);
        //assert(0 == x_dts || dts >= x_dts);
        printf("[X] pts: %s(%" PRId64 "), dts: %s(%" PRId64 "), diff: %03d/%03d\n", ftimestamp(pts, s_pts), pts, ftimestamp(dts, s_dts), dts, (int)(pts - it->second.first), (int)(dts - it->second.second));
    }

    it->second = std::make_pair(pts, dts);
    return 0;
}

static void mpeg_ps_dec_testonstream(void* param, int stream, int codecid, const void* extra, int bytes, int finish)
{
    printf("stream %d, codecid: %d, finish: %s\n", stream, codecid, finish ? "true" : "false");
}

static uint8_t s_packet[2 * 1024 * 1024];

void mpeg_ps_dec_test(const char* file)
{
    FILE* fp = fopen(file, "rb");
    vfp = fopen("v.h264", "wb");
    afp = fopen("a.aac", "wb");

    struct ps_demuxer_notify_t notify = {
        mpeg_ps_dec_testonstream,
    };
    ps_demuxer_t* ps = ps_demuxer_create(onpacket, NULL);
    ps_demuxer_set_notify(ps, &notify, NULL);

    size_t n, i = 0, r = 0;
    while ((n = fread(s_packet + i, 1, sizeof(s_packet) - i, fp)) > 0)
    {
        r = ps_demuxer_input(ps, s_packet, n + i);
        assert(r == n + i);
        memmove(s_packet, s_packet + r, n + i - r);
        i = n + i - r;
    }
    while (i > 0 && r > 0)
    {
        r = ps_demuxer_input(ps, s_packet, i);
        memmove(s_packet, s_packet + r, i - r);
        i -= r;
    }
    ps_demuxer_destroy(ps);

    fclose(fp);
    fclose(vfp);
    fclose(afp);
}
