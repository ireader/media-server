#include "mov-reader.h"
#include "mov-format.h"
#include "mpeg4-hevc.h"
#include "mpeg4-avc.h"
#include "mpeg4-aac.h"
#include "webm-vpx.h"
#include "aom-av1.h"
#include "rtp-payload.h"
#include "rtp-profile.h"
#include "mpeg-ps.h"
#include "mpeg-ts.h"
#include "mpeg-ts-proto.h"
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
    int psi;
    int64_t dts;
    
    void* encoder;
    void* decoder;
};

struct mov_rtp_test_t
{
    struct mov_rtp_test_stream_t a, v;
    
    struct ps_muxer_t* psenc;
    struct ps_demuxer_t* psdec;
};

static unsigned char packet[8 * 1024 * 1024];

static void* rtp_alloc(void* /*param*/, int bytes)
{
    static uint8_t buffer[2 * 1024 * 1024 + 4] = { 0, 0, 0, 1, };
    assert(bytes <= sizeof(buffer) - 4);
    return buffer + 4;
}

static void rtp_free(void* /*param*/, void * /*packet*/)
{
}

static int rtp_encode_packet(void* param, const void *packet, int bytes, uint32_t timestamp, int /*flags*/)
{
    struct mov_rtp_test_stream_t* ctx = (struct mov_rtp_test_stream_t*)param;
    
    int r = rand();
    if( (r % 100) < RTP_LOST_PERCENT )
    {
        printf("======== discard [%s] timestamp: %u ==============\n", ctx->av ? "V" : "A", (unsigned int)timestamp);
        return 0;
    }
    
    r = rtp_payload_decode_input(ctx->decoder, packet, bytes);
    return r >= 0 ? 0 : r;
}

static int rtp_decode_packet(void* param, const void *packet, int bytes, uint32_t timestamp, int flags)
{
    struct mov_rtp_test_stream_t* ctx = (struct mov_rtp_test_stream_t*)param;
    printf("RTP Decode: [%s] timestamp: %u, bytes: %d\n", ctx->av ? "V" : "A", (unsigned int)timestamp, bytes);
    
#if defined(RTP_VIDEO_WITH_PS)
    if(ctx == &ctx->ctx->v)
    {
        size_t r = ps_demuxer_input(ctx->ctx->psdec, (const uint8_t*)packet, bytes);
        assert(r == bytes);
        return r;
    }
#endif

    return 0;
}

static int rtp_payload_codec_create(struct mov_rtp_test_stream_t* ctx, int payload, const char* encoding, uint16_t seq, uint32_t ssrc)
{
    struct rtp_payload_t handler1;
    handler1.alloc = rtp_alloc;
    handler1.free = rtp_free;
    handler1.packet = rtp_decode_packet;
    ctx->decoder = rtp_payload_decode_create(payload, encoding, &handler1, ctx);
    
    struct rtp_payload_t handler2;
    handler2.alloc = rtp_alloc;
    handler2.free = rtp_free;
    handler2.packet = rtp_encode_packet;
    ctx->encoder = rtp_payload_encode_create(payload, encoding, seq, ssrc, &handler2, ctx);

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
    int n;

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
        else if (MOV_OBJECT_VP9 == ctx->v.object)
        {
            n = aom_av1_codec_configuration_record_save(&s_av1, s_packet, sizeof(s_packet));
        }
        else
        {
            assert(0);
        }
        
        printf("[V] pts: %s, dts: %s, diff: %03d/%03d, %d%s\n", ftimestamp(pts, s_pts), ftimestamp(dts, s_dts), (int)(pts - v_pts), (int)(dts - v_dts), (int)n, flags ? " [I]" : "");
        v_pts = pts;
        v_dts = dts;
#if defined(RTP_VIDEO_WITH_PS)
        if(MOV_OBJECT_H264 == ctx->v.object || MOV_OBJECT_HEVC == ctx->v.object)
        {
            ctx->v.dts = dts;
            ps_muxer_input(ctx->psenc, ctx->v.psi, (MOV_AV_FLAG_KEYFREAME&flags)?0x0001:0, pts, dts, s_packet, n);
            return;
        }
#endif
        assert(0 == rtp_payload_encode_input(ctx->v.encoder, s_packet, n, (unsigned int)dts));
    }
    else if (ctx->a.track == track)
    {
        if (MOV_OBJECT_AAC == ctx->a.object)
        {
            n = mpeg4_aac_adts_save(&s_aac, bytes, s_packet, sizeof(s_packet));
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
        assert(0 == rtp_payload_encode_input(ctx->a.encoder, s_packet, n, (unsigned int)dts));
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
        ctx->v.psi = ps_muxer_add_stream(ctx->psenc, PSI_STREAM_H264, NULL, 0);
#else
        assert(0 == rtp_payload_codec_create(&ctx->v, 96, "H264", 0, 0));
#endif
        assert(bytes == mpeg4_avc_decoder_configuration_record_load((const uint8_t*)extra, bytes, &s_avc));
        
    }
    else if (MOV_OBJECT_HEVC == object)
    {
#if defined(RTP_VIDEO_WITH_PS)
        ctx->v.psi = ps_muxer_add_stream(ctx->psenc, PSI_STREAM_H265, NULL, 0);
#else
        assert(0 == rtp_payload_codec_create(&ctx->v, 96, "H265", 0, 0));
#endif
        assert(bytes == mpeg4_hevc_decoder_configuration_record_load((const uint8_t*)extra, bytes, &s_hevc));
    }
    else if (MOV_OBJECT_AV1 == object)
    {
        assert(0 == rtp_payload_codec_create(&ctx->v, 96, "AV1", 0, 0));
        assert(bytes == aom_av1_codec_configuration_record_load((const uint8_t*)extra, bytes, &s_av1));
    }
    else if (MOV_OBJECT_VP9 == object)
    {
        assert(0 == rtp_payload_codec_create(&ctx->v, 96, "VP9", 0, 0));
        assert(bytes == webm_vpx_codec_configuration_record_load((const uint8_t*)extra, bytes, &s_vpx));
    }
    else
    {
        assert(0);
    }
}

static void mov_audio_info(void* param, uint32_t track, uint8_t object, int /*channel_count*/, int /*bit_per_sample*/, int /*sample_rate*/, const void* extra, size_t bytes)
{
    struct mov_rtp_test_t* ctx = (struct mov_rtp_test_t*)param;
    ctx->a.track = track;
    ctx->a.object = object;
    ctx->a.av = 0;
    
    if (MOV_OBJECT_AAC == object)
    {
        assert(0 == rtp_payload_codec_create(&ctx->a, 97, "MP4A-LATM", 0, 0));
        assert(bytes == mpeg4_aac_audio_specific_config_load((const uint8_t*)extra, bytes, &s_aac));
    }
    else if (MOV_OBJECT_OPUS == object)
    {
        assert(0 == rtp_payload_codec_create(&ctx->a, 97, "OPUS", 0, 0));
    }
    else
    {
        assert(0);
    }
}

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

static int ps_write(void* param, int stream, void* packet, size_t bytes)
{
    struct mov_rtp_test_t* ctx = (struct mov_rtp_test_t*)param;
    return rtp_payload_encode_input(ctx->v.encoder, packet, bytes, (unsigned int)ctx->v.dts);
}

static int ps_onpacket(void* ps, int stream, int codecid, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes)
{
    printf("PS Decode [V] pts: %08lu, dts: %08lu, bytes: %u, %s\n", (unsigned long)pts, (unsigned long)dts, (unsigned int)bytes, flags ? " [I]" : "");
    return 0;
}

void mov_rtp_test(const char* mp4)
{
    struct mov_rtp_test_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.a.ctx = &ctx;
    ctx.v.ctx = &ctx;
    
#if defined(RTP_VIDEO_WITH_PS)
    struct ps_muxer_func_t handler;
    handler.alloc = ps_alloc;
    handler.write = ps_write;
    handler.free = ps_free;
    ctx.psenc = ps_muxer_create(&handler, &ctx);
    ctx.psdec = ps_demuxer_create(ps_onpacket, &ctx);
    assert(0 == rtp_payload_codec_create(&ctx.v, 96, "MP2P", 0, 0));
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

    if(ctx.a.decoder)
        rtp_payload_decode_destroy(ctx.a.decoder);
    if(ctx.a.encoder)
        rtp_payload_encode_destroy(ctx.a.encoder);
    if(ctx.v.decoder)
        rtp_payload_decode_destroy(ctx.v.decoder);
    if(ctx.v.encoder)
        rtp_payload_encode_destroy(ctx.v.encoder);
    
    ps_demuxer_destroy(ctx.psdec);
    ps_muxer_destroy(ctx.psenc);
    mov_reader_destroy(mov);
    fclose(fp);
}
