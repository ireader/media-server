#include "mov-reader.h"
#include "mov-format.h"
#include "mpeg4-hevc.h"
#include "mpeg4-avc.h"
#include "mpeg4-aac.h"
#include "webm-vpx.h"
#include "aom-av1.h"
#include "rtp-profile.h"
#include "rtsp-muxer.h"
#include "sockutil.h"
#include "sys/system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#define IP "127.0.0.1"

#define SOCKET_STORAGE_TO_ADDR(storage) (const struct sockaddr*)(storage), socket_addr_len((const struct sockaddr*)(storage))

extern "C" const struct mov_buffer_t* mov_file_buffer(void);

static uint8_t s_packet[2 * 1024 * 1024];
static uint8_t s_buffer[4 * 1024 * 1024];
static struct mpeg4_hevc_t s_hevc;
static struct mpeg4_avc_t s_avc;
static struct mpeg4_aac_t s_aac;
static struct webm_vpx_t s_vpx;
static struct aom_av1_t s_av1;

struct rtp_streaming_test_t;
struct rtp_streaming_test_stream_t
{
    struct rtp_streaming_test_t* ctx;

    int av;
    int object;
    int track;
    int psi;
    int64_t dts;

    int mid;
    struct rtsp_muxer_t* rtp;

    socket_t udp[2];
    struct sockaddr_storage addr[2];
};

struct rtp_streaming_test_t
{
    struct rtp_streaming_test_stream_t a, v;
    uint64_t clock;
};

static int rtp_encode_packet(void* param, int pid, const void* packet, int bytes, uint32_t timestamp, int /*flags*/)
{
    static uint8_t rtcp[1500];
    struct rtp_streaming_test_stream_t* ctx = (struct rtp_streaming_test_stream_t*)param;
    assert(bytes == socket_sendto(ctx->udp[0], packet, bytes, 0, SOCKET_STORAGE_TO_ADDR(&ctx->addr[0])));

    int r = rtsp_muxer_rtcp(ctx->rtp, ctx->mid, rtcp, sizeof(rtcp));
    if (r > 0)
    {
        assert(r == socket_sendto(ctx->udp[1], rtcp, r, 0, SOCKET_STORAGE_TO_ADDR(&ctx->addr[1])));
    }

    return 0;
}

static inline const char* ftimestamp(int64_t timestamp, char* buf)
{
    uint32_t t = (uint32_t)timestamp;
    sprintf(buf, "%02u:%02u:%02u.%03u", t / 3600000, (t / 60000) % 60, (t / 1000) % 60, t % 1000);
    return buf;
}

static void onread(void* param, uint32_t track, const void* buffer, size_t bytes, int64_t pts, int64_t dts, int flags)
{
    static char s_pts[64], s_dts[64];
    static int64_t v_pts, v_dts;
    static int64_t a_pts, a_dts;
    struct rtp_streaming_test_t* ctx = (struct rtp_streaming_test_t*)param;

    uint64_t clock = system_clock();
    if (clock - ctx->clock + 5 < dts)
        system_sleep(dts - (clock - ctx->clock + 5));

    if (ctx->v.track == track)
    {
        if (MOV_OBJECT_H264 == ctx->v.object)
        {
            bytes = h264_mp4toannexb(&s_avc, buffer, bytes, s_packet, sizeof(s_packet));
            buffer = s_packet;
        }
        else if (MOV_OBJECT_HEVC == ctx->v.object)
        {
            bytes = h265_mp4toannexb(&s_hevc, buffer, bytes, s_packet, sizeof(s_packet));
            buffer = s_packet;
        }
        else if (MOV_OBJECT_AV1 == ctx->v.object)
        {
            //n = aom_av1_codec_configuration_record_save(&s_av1, s_packet, sizeof(s_packet));
        }
        else if (MOV_OBJECT_VP9 == ctx->v.object || MOV_OBJECT_VP8 == ctx->v.object)
        {
            //n = aom_av1_codec_configuration_record_save(&s_av1, s_packet, sizeof(s_packet));
        }
        else
        {
            assert(0);
        }

        printf("[V] pts: %s, dts: %s, diff: %03d/%03d, %d%s\n", ftimestamp(pts, s_pts), ftimestamp(dts, s_dts), (int)(pts - v_pts), (int)(dts - v_dts), (int)bytes, flags ? " [I]" : "");
        v_pts = pts;
        v_dts = dts;
        assert(0 == rtsp_muxer_input(ctx->v.rtp, ctx->v.mid, pts, dts, buffer, bytes, 0));
    }
    else if (ctx->a.track == track)
    {
        if (MOV_OBJECT_AAC == ctx->a.object)
        {
            bytes = mpeg4_aac_adts_save(&s_aac, bytes, s_packet, sizeof(s_packet));
            buffer = s_packet;
        }
        else if (MOV_OBJECT_OPUS == ctx->a.object)
        {
        }
        else
        {
            assert(0);
        }

        printf("[A] pts: %s, dts: %s, diff: %03d/%03d, %d\n", ftimestamp(pts, s_pts), ftimestamp(dts, s_dts), (int)(pts - a_pts), (int)(dts - a_dts), (int)bytes);
        a_pts = pts;
        a_dts = dts;
        assert(0 == rtsp_muxer_input(ctx->a.rtp, ctx->a.mid, pts, dts, buffer, bytes, 0));
    }
    else
    {
        assert(0);
    }
}

static void mov_video_info(void* param, uint32_t track, uint8_t object, int /*width*/, int /*height*/, const void* extra, size_t bytes)
{
    struct rtp_streaming_test_t* ctx = (struct rtp_streaming_test_t*)param;
    ctx->v.track = track;
    ctx->v.object = object;
    ctx->v.av = 1;
    ctx->v.udp[0] = socket_udp_bind_ipv4(NULL, 0);
    ctx->v.udp[1] = socket_udp_bind_ipv4(NULL, 0);
    assert(0 == socket_addr_from(&ctx->v.addr[0], NULL, IP, 8004));
    assert(0 == socket_addr_from(&ctx->v.addr[1], NULL, IP, 8005));
    ctx->v.rtp = rtsp_muxer_create(rtp_encode_packet, &ctx->v);

    if (MOV_OBJECT_H264 == object)
    {
        assert(bytes == mpeg4_avc_decoder_configuration_record_load((const uint8_t*)extra, bytes, &s_avc));
        int pid = rtsp_muxer_add_payload(ctx->v.rtp, "RTP/AVP", 90000, 126, "H264", 0, 0, 0, extra, bytes);
        ctx->v.mid = rtsp_muxer_add_media(ctx->v.rtp, pid, RTP_PAYLOAD_H264, extra, bytes);
    }
    else if (MOV_OBJECT_HEVC == object)
    {
        assert(bytes == mpeg4_hevc_decoder_configuration_record_load((const uint8_t*)extra, bytes, &s_hevc));
        int pid = rtsp_muxer_add_payload(ctx->v.rtp, "RTP/AVP", 90000, RTP_PAYLOAD_H265, "H265", 0, 0, 0, extra, bytes);
        ctx->v.mid = rtsp_muxer_add_media(ctx->v.rtp, pid, RTP_PAYLOAD_H265, extra, bytes);
    }
    else if (MOV_OBJECT_AV1 == object)
    {
        assert(bytes == aom_av1_codec_configuration_record_load((const uint8_t*)extra, bytes, &s_av1));
        int pid = rtsp_muxer_add_payload(ctx->v.rtp, "RTP/AVP", 90000, RTP_PAYLOAD_AV1X, "AV1X", 0, 0, 0, extra, bytes);
        ctx->v.mid = rtsp_muxer_add_media(ctx->v.rtp, pid, RTP_PAYLOAD_AV1X, extra, bytes);
    }
    else if (MOV_OBJECT_VP9 == object)
    {
        assert(bytes == webm_vpx_codec_configuration_record_load((const uint8_t*)extra, bytes, &s_vpx));
        int pid = rtsp_muxer_add_payload(ctx->v.rtp, "RTP/AVP", 90000, RTP_PAYLOAD_VP9, "VP9", 0, 0, 0, extra, bytes);
        ctx->v.mid = rtsp_muxer_add_media(ctx->v.rtp, pid, RTP_PAYLOAD_VP9, extra, bytes);
    }
    else if (MOV_OBJECT_VP8 == object)
    {
        assert(bytes == webm_vpx_codec_configuration_record_load((const uint8_t*)extra, bytes, &s_vpx));
        int pid = rtsp_muxer_add_payload(ctx->v.rtp, "RTP/AVP", 90000, 100, "VP8", 0, 0, 0, extra, bytes);
        ctx->v.mid = rtsp_muxer_add_media(ctx->v.rtp, pid, RTP_PAYLOAD_VP8, extra, bytes);
    }
    else
    {
        assert(0);
    }
}

static void mov_audio_info(void* param, uint32_t track, uint8_t object, int /*channel_count*/, int /*bit_per_sample*/, int sample_rate, const void* extra, size_t bytes)
{
    struct rtp_streaming_test_t* ctx = (struct rtp_streaming_test_t*)param;
    ctx->a.track = track;
    ctx->a.object = object;
    ctx->a.av = 0;
    ctx->a.udp[0] = socket_udp_bind_ipv4(NULL, 0);
    ctx->a.udp[1] = socket_udp_bind_ipv4(NULL, 0);
    assert(0 == socket_addr_from(&ctx->a.addr[0], NULL, IP, 5002));
    assert(0 == socket_addr_from(&ctx->a.addr[1], NULL, IP, 5003));
    ctx->a.rtp = rtsp_muxer_create(rtp_encode_packet, &ctx->a);

    if (MOV_OBJECT_AAC == object)
    {
        assert(bytes == mpeg4_aac_audio_specific_config_load((const uint8_t*)extra, bytes, &s_aac));
        int pid = rtsp_muxer_add_payload(ctx->a.rtp, "RTP/AVP", sample_rate, RTP_PAYLOAD_LATM, "MP4A-LATM", 0, 0, 0, extra, bytes);
        ctx->a.mid = rtsp_muxer_add_media(ctx->a.rtp, pid, RTP_PAYLOAD_LATM, extra, bytes);
    }
    else if (MOV_OBJECT_OPUS == object)
    {
        assert(48000 == sample_rate);
        int pid = rtsp_muxer_add_payload(ctx->a.rtp, "RTP/AVP", sample_rate, 111, "OPUS", 0, 0, 0, extra, bytes);
        ctx->a.mid = rtsp_muxer_add_media(ctx->a.rtp, pid, RTP_PAYLOAD_OPUS, extra, bytes);
    }
    else
    {
        assert(0);
    }
}

void rtp_streaming_test(const char* mp4)
{
    struct rtp_streaming_test_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.a.ctx = &ctx;
    ctx.v.ctx = &ctx;

    FILE* fp = fopen(mp4, "rb");
    mov_reader_t* mov = mov_reader_create(mov_file_buffer(), fp);
    uint64_t duration = mov_reader_getduration(mov);

    struct mov_reader_trackinfo_t info = { mov_video_info, mov_audio_info };
    mov_reader_getinfo(mov, &info, &ctx);

    ctx.clock = system_clock();
    while (mov_reader_read(mov, s_buffer, sizeof(s_buffer), onread, &ctx) > 0)
    {
        int n = 0;
        socket_t udp[4];
        if (ctx.v.udp[0] && socket_invalid != ctx.v.udp[0])
        {
            udp[n] = ctx.v.udp[0];
            udp[n++] = ctx.v.udp[1];
        }

        if (ctx.a.udp[0] && socket_invalid != ctx.a.udp[0])
        {
            udp[n] = ctx.a.udp[0];
            udp[n++] = ctx.a.udp[1];
        }

        socklen_t addrlen;
        struct sockaddr_storage addr;
        int64_t flags = socket_poll_readv(0, n, udp);
        for (int i = 0; i < 4; i++)
        {
            // discard rtcp
            if (flags & (1LL << i))
            {
                socket_recvfrom(udp[i], s_buffer, sizeof(s_buffer), 0, (struct sockaddr*)&addr, &addrlen);
            }
        }
    }

    if (ctx.a.rtp)
        rtsp_muxer_destroy(ctx.a.rtp);
    if (ctx.v.rtp)
        rtsp_muxer_destroy(ctx.v.rtp);
   
    mov_reader_destroy(mov);
    fclose(fp);
}
