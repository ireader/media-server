#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <inttypes.h>
#include "rtp-demuxer.h"
#include "rtsp-demuxer.h"
#include "avpkt2bs.h"

#define USE_RTP_DEMUXER 0

struct rtsp_demuxer_test_t
{
#if USE_RTP_DEMUXER
    struct rtp_demuxer_t* demuxer;
#else
    struct rtsp_demuxer_t* demuxer;
    struct avpkt2bs_t bs;
#endif

    FILE* fp;
};

#if USE_RTP_DEMUXER
static int rtp_demuxer_test_onpacket(void* param, const void* packet, int bytes, uint32_t timestamp, int flags)
{
    struct rtsp_demuxer_test_t* ctx = (struct rtsp_demuxer_test_t*)param;
    return bytes == fwrite(packet, 1, bytes, ctx->fp) ? 0 : ferror(ctx->fp);
}
#else
static int rtsp_demuxer_test_onpacket(void* param, struct avpacket_t* pkt)
{
    struct rtsp_demuxer_test_t* ctx = (struct rtsp_demuxer_test_t*)param;
    static int64_t s_dts = 0;
    if (0 == s_dts)
        s_dts = pkt->dts;
    printf("[%d:0x%x] pts: %" PRId64 ", dts: %" PRId64 ", cts: %" PRId64 ", diff: %" PRId64 ", bytes: %d\n", pkt->stream->stream, (unsigned int)pkt->stream->codecid, pkt->pts, pkt->dts, pkt->pts - pkt->dts, pkt->dts - s_dts, pkt->size);
    s_dts = pkt->dts;

    int r = avpkt2bs_input(&ctx->bs, pkt);
    fwrite(ctx->bs.ptr, 1, r, ctx->fp);
    //fwrite(pkt->data, 1, pkt->size, ctx->fp);
    return 0;
}
#endif

void rstp_demuxer_test(int payload, const char* encoding, uint16_t seq, uint32_t ssrc, const char* rtpfile)
{
    uint8_t buffer[1500];
    FILE* fp = fopen(rtpfile, "rb");
    assert(fp);

    struct rtsp_demuxer_test_t ctx;
    ctx.fp = fopen("rtp.bin", "wb");
    
#if USE_RTP_DEMUXER
    ctx.demuxer = rtp_demuxer_create(100, 90000, payload, encoding, rtp_demuxer_test_onpacket, &ctx);
#else
    avpkt2bs_create(&ctx.bs);
    ctx.demuxer = rtsp_demuxer_create(0, 100, rtsp_demuxer_test_onpacket, &ctx);
    rtsp_demuxer_add_payload(ctx.demuxer, 90000, payload, encoding, "96 profile-level-id=1; cpresent=0; config=400023203fc0;");
    rtsp_demuxer_rtpinfo(ctx.demuxer, seq, ssrc);
#endif
    
    while (1)
    {
        uint8_t s2[2];
        if (2 != fread(s2, 1, 2, fp))
            break;

        int size = (s2[0] << 8) | s2[1];
        assert(size < sizeof(buffer));
        assert(size == (int)fread(buffer, 1, size, fp));
#if USE_RTP_DEMUXER
        int r = rtp_demuxer_input(ctx.demuxer, buffer, size);
#else
        int r = rtsp_demuxer_input(ctx.demuxer, buffer, size);
        
#endif
        assert(r >= 0);
        
    }

#if USE_RTP_DEMUXER
    rtp_demuxer_destroy(&ctx.demuxer);
#else
    rtsp_demuxer_destroy(ctx.demuxer);
    avpkt2bs_destroy(&ctx.bs);
#endif
    
    fclose(fp);
    fclose(ctx.fp);
}
