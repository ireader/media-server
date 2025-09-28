#include "mov-reader.h"
#include "mov-format.h"
#include "mpeg4-hevc.h"
#include "mpeg4-avc.h"
#include "mpeg4-aac.h"
#include "webm-vpx.h"
#include "aom-av1.h"
#include "rtp-payload.h"
#include "rtp-profile.h"
#include "rtsp-muxer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#define RTP_VIDEO_WITH_PS

#define RTP_LOST_PERCENT 5

extern "C" const struct mov_buffer_t* mov_file_buffer(void);

static uint8_t s_packet[2 * 1024 * 1024];
static uint8_t s_buffer[4 * 1024 * 1024];
static struct mpeg4_hevc_t s_hevc;
static struct mpeg4_avc_t s_avc;
static struct mpeg4_aac_t s_aac;
static struct webm_vpx_t s_vpx;
static struct aom_av1_t s_av1;

struct mov_rtp_test_t;
struct mov_rtp_test_stream_t
{
    struct mov_rtp_test_t* ctx;
    
    int av;
    int object;
    int track;
    int mid;
    int64_t dts;
};

struct mov_rtp_test_t
{
    struct mov_rtp_test_stream_t a, v;
    
    struct rtsp_muxer_t* muxer;
    int rtp_ps_pid;
};

static unsigned char packet[8 * 1024 * 1024];

static int rtp_encode_packet(void* param, int pid, const void* data, int bytes, uint32_t timestamp, int flags)
{
    struct mov_rtp_test_stream_t* ctx = (struct mov_rtp_test_stream_t*)param;
    
    //int x = rand();
    //if( (x % 100) < RTP_LOST_PERCENT )
    //{
    //    printf("======== discard [%s] timestamp: %u ==============\n", ctx->av ? "V" : "A", (unsigned int)timestamp);
    //    return 0;
    //}
    
    //int r = rtp_payload_decode_input(ctx->decoder, packet, bytes);
    //return r >= 0 ? 0 : r;

    uint8_t len[2];
    static FILE* fp = fopen("2.mp4.rtp", "wb");
    len[0] = bytes >> 8;
    len[1] = bytes;
    fwrite(len, 1, 2, fp);
    fwrite(data, 1, bytes, fp);
    fflush(fp);
    return 0;
}

inline const char* ftimestamp(uint32_t t, char* buf)
{
    sprintf(buf, "%02u:%02u:%02u.%03u", t / 3600000, (t / 60000) % 60, (t / 1000) % 60, t % 1000);
    return buf;
}

static void onread(void* param, uint32_t track, const void* buffer, size_t bytes, int64_t pts, int64_t dts, int flags)
{
    static char s_pts[64], s_dts[64];
    static int64_t v_pts, v_dts;
    static int64_t a_pts, a_dts;
    struct mov_rtp_test_t* ctx = (struct mov_rtp_test_t*)param;
    int n = bytes;

    if (ctx->v.track == track)
    {
        if (MOV_OBJECT_H264 == ctx->v.object)
        {
            n = h264_mp4toannexb(&s_avc, buffer, bytes, s_packet, sizeof(s_packet));
        }
        else if (MOV_OBJECT_HEVC == ctx->v.object)
        {
            n = h265_mp4toannexb(&s_hevc, buffer, bytes, s_packet, sizeof(s_packet));
        }
        else if (MOV_OBJECT_AV1 == ctx->v.object)
        {
            n = aom_av1_codec_configuration_record_save(&s_av1, s_packet, sizeof(s_packet));
        }
        else if (MOV_OBJECT_VP8 == ctx->v.object || MOV_OBJECT_VP9 == ctx->v.object)
        {
            // nothing to do
        }
        else
        {
            assert(0);
        }
        
        printf("[V] pts: %s, dts: %s, diff: %03d/%03d, %d%s\n", ftimestamp(pts, s_pts), ftimestamp(dts, s_dts), (int)(pts - v_pts), (int)(dts - v_dts), (int)n, flags ? " [I]" : "");
        v_pts = pts;
        v_dts = dts;
        assert(0 == rtsp_muxer_input(ctx->muxer, ctx->v.mid, pts, dts, s_packet, n, (MOV_AV_FLAG_KEYFREAME & flags) ? 0x0001 : 0));
    }
    else if (ctx->a.track == track)
    {
        if (MOV_OBJECT_AAC == ctx->a.object)
        {
            n = mpeg4_aac_adts_save(&s_aac, bytes, s_packet, sizeof(s_packet));
            memcpy(s_packet + n, buffer, bytes);
            n += bytes;
        }
        else if(MOV_OBJECT_OPUS == ctx->a.object)
        {
            assert(0);
        }
        else
        {
            assert(0);
        }
        
        printf("[A] pts: %s, dts: %s, diff: %03d/%03d, %d\n", ftimestamp(pts, s_pts), ftimestamp(dts, s_dts), (int)(pts - a_pts), (int)(dts - a_dts), (int)n);
        a_pts = pts;
        a_dts = dts;
        assert(0 == rtsp_muxer_input(ctx->muxer, ctx->a.mid, pts, dts, s_packet, n, 0));
    }
    else
    {
        assert(0);
    }
}

static void mov_video_info(void* param, uint32_t track, uint8_t object, int /*width*/, int /*height*/, const void* extra, size_t bytes )
{
    struct mov_rtp_test_t* ctx = (struct mov_rtp_test_t*)param;
    ctx->v.track = track;
    ctx->v.object = object;
    ctx->v.av = 1;
    
    if (MOV_OBJECT_H264 == object)
    {
#if defined(RTP_VIDEO_WITH_PS)
        int pid = ctx->rtp_ps_pid;
#else   
        int pid = rtsp_muxer_add_payload(ctx->muxer, "RTP/AVP", 90000, 96, "H264", 0, 0, 0, extra, bytes);
#endif
        ctx->v.mid = rtsp_muxer_add_media(ctx->muxer, pid, RTP_PAYLOAD_H264, extra, bytes);
        assert(bytes >= mpeg4_avc_decoder_configuration_record_load((const uint8_t*)extra, bytes, &s_avc));
        
    }
    else if (MOV_OBJECT_HEVC == object)
    {
#if defined(RTP_VIDEO_WITH_PS)
        int pid = ctx->rtp_ps_pid;
#else   
        int pid = rtsp_muxer_add_payload(ctx->muxer, "RTP/AVP", 90000, 96, "H265", 0, 0, 0, extra, bytes);
#endif
        ctx->v.mid = rtsp_muxer_add_media(ctx->muxer, pid, RTP_PAYLOAD_H265, extra, bytes);
        assert(bytes == mpeg4_hevc_decoder_configuration_record_load((const uint8_t*)extra, bytes, &s_hevc));
    }
    else if (MOV_OBJECT_AV1 == object)
    {
        int pid = rtsp_muxer_add_payload(ctx->muxer, "RTP/AVP", 90000, RTP_PAYLOAD_AV1X, "AV1X", 0, 0, 0, extra, bytes);
        ctx->v.mid = rtsp_muxer_add_media(ctx->muxer, pid, RTP_PAYLOAD_AV1X, extra, bytes);
        assert(bytes == aom_av1_codec_configuration_record_load((const uint8_t*)extra, bytes, &s_av1));
    }
    else if (MOV_OBJECT_VP9 == object)
    {
        int pid = rtsp_muxer_add_payload(ctx->muxer, "RTP/AVP", 90000, 96, "VP9", 0, 0, 0, extra, bytes);
        ctx->v.mid = rtsp_muxer_add_media(ctx->muxer, pid, RTP_PAYLOAD_VP9, extra, bytes);
        assert(bytes == webm_vpx_codec_configuration_record_load((const uint8_t*)extra, bytes, &s_vpx));
    }
    else if (MOV_OBJECT_VP8 == object)
    {
        int pid = rtsp_muxer_add_payload(ctx->muxer, "RTP/AVP", 90000, 96, "VP8", 0, 0, 0, extra, bytes);
        ctx->v.mid = rtsp_muxer_add_media(ctx->muxer, pid, RTP_PAYLOAD_VP8, extra, bytes);
        assert(bytes == webm_vpx_codec_configuration_record_load((const uint8_t*)extra, bytes, &s_vpx));
    }
    else
    {
        assert(0);
    }
}

static void mov_audio_info(void* param, uint32_t track, uint8_t object, int /*channel_count*/, int /*bit_per_sample*/, int sample_rate, const void* extra, size_t bytes)
{
    struct mov_rtp_test_t* ctx = (struct mov_rtp_test_t*)param;
    ctx->a.track = track;
    ctx->a.object = object;
    ctx->a.av = 0;
    
    if (MOV_OBJECT_AAC == object)
    {
#if defined(RTP_VIDEO_WITH_PS)
        int pid = ctx->rtp_ps_pid;
#else   
        int pid = rtsp_muxer_add_payload(ctx->muxer, "RTP/AVP", sample_rate, 97, "MP4A-LATM", 0, 0, 0, extra, bytes);
#endif
        ctx->a.mid = rtsp_muxer_add_media(ctx->muxer, pid, RTP_PAYLOAD_LATM, extra, bytes);
        assert(bytes == mpeg4_aac_audio_specific_config_load((const uint8_t*)extra, bytes, &s_aac));
    }
    else if (MOV_OBJECT_OPUS == object)
    {
        int pid = rtsp_muxer_add_payload(ctx->muxer, "RTP/AVP", sample_rate, 97, "OPUS", 0, 0, 0, extra, bytes);
        ctx->a.mid = rtsp_muxer_add_media(ctx->muxer, pid, RTP_PAYLOAD_OPUS, extra, bytes);
    }
    else
    {
        assert(0);
    }
}

void mov_rtp_test(const char* mp4)
{
    struct mov_rtp_test_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.a.ctx = &ctx;
    ctx.v.ctx = &ctx;
    ctx.muxer = rtsp_muxer_create(rtp_encode_packet, &ctx);

#if defined(RTP_VIDEO_WITH_PS)
    ctx.rtp_ps_pid = rtsp_muxer_add_payload(ctx.muxer, "RTP/AVP", 90000, 96, "MP2P", 0, 0, 0, NULL, 0);
#endif
    
    FILE* fp = fopen(mp4, "rb");
    mov_reader_t* mov = mov_reader_create(mov_file_buffer(), fp);
    uint64_t duration = mov_reader_getduration(mov);

    struct mov_reader_trackinfo_t info = { mov_video_info, mov_audio_info };
    mov_reader_getinfo(mov, &info, &ctx);

    //srand((int)time(NULL));
    while (mov_reader_read(mov, s_buffer, sizeof(s_buffer), onread, &ctx) > 0)
    {
    }

    rtsp_muxer_destroy(ctx.muxer);
    mov_reader_destroy(mov);
    fclose(fp);
}
