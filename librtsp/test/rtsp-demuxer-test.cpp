#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <inttypes.h>
#include "rtp-demuxer.h"
#include "rtsp-demuxer.h"

#define USE_RTP_DEMUXER 0

#if USE_RTP_DEMUXER
static int rtp_demuxer_test_onpacket(void* param, const void* packet, int bytes, uint32_t timestamp, int flags)
{
    return bytes == fwrite(packet, 1, bytes, (FILE*)param) ? 0 : ferror((FILE*)param);
}
#else
static int rtsp_demuxer_test_onpacket(void* param, struct avpacket_t* pkt)
{
    static int64_t s_dts = 0;
    if (0 == s_dts)
        s_dts = pkt->dts;
    printf("[%d:0x%x] pts: %" PRId64 ", dts: %" PRId64 ", cts: %" PRId64 ", diff: %" PRId64 ", bytes: %d\n", pkt->stream->stream, (unsigned int)pkt->stream->codecid, pkt->pts, pkt->dts, pkt->pts - pkt->dts, pkt->dts - s_dts, pkt->size);
    s_dts = pkt->dts;

    fwrite(pkt->data, 1, pkt->size, (FILE*)param);
    return 0;
}
#endif

void rstp_demuxer_test(int payload, const char* encoding, uint16_t seq, uint32_t ssrc, const char* rtpfile)
{
    uint8_t buffer[1500];
    FILE* fp = fopen(rtpfile, "rb");
    FILE* wfp = fopen("xx.ps", "wb");
    assert(fp);
 
#if USE_RTP_DEMUXER
    struct rtp_demuxer_t* demuxer = rtp_demuxer_create(100, 90000, payload, encoding, rtp_demuxer_test_onpacket, wfp);
#else
    struct rtsp_demuxer_t* demuxer = rtsp_demuxer_create(0, 100, rtsp_demuxer_test_onpacket, wfp);
    rtsp_demuxer_add_payload(demuxer, 90000, payload, encoding, "96 profile-level-id=1; cpresent=0; config=400023203fc0;");
    rtsp_demuxer_rtpinfo(demuxer, seq, ssrc);
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
        assert(rtp_demuxer_input(demuxer, buffer, size) >= 0);
#else
        assert(rtsp_demuxer_input(demuxer, buffer, size) >= 0);
#endif
        
    }

#if USE_RTP_DEMUXER
    rtp_demuxer_destroy(&demuxer);
#else
    rtsp_demuxer_destroy(demuxer);
#endif
    
    fclose(fp);
    fclose(wfp);
}
