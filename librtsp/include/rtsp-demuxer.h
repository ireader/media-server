#ifndef _rtsp_demuxer_h_
#define _rtsp_demuxer_h_

#include "rtp-demuxer.h"
#include "rtp-profile.h"
#include "sdp-a-fmtp.h"
#include "mpeg-ts-proto.h"
#include "mpeg-ps.h"
#include "mpeg-ts.h"
#include "cstringext.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

struct rtsp_demuxer_t
{
    void* param;
    int (*onpacket)(void* param, struct rtsp_demuxer_t* demuxer, int stream, int payload, const char* encoding, int64_t pts, int64_t dts, const void *data, size_t bytes, int flags);
    
    struct rtp_demuxer_t* rtp;
    void *ps, *ts; // only one
    
    int payload;
    char encoding[64];
    
    int stream; // stream index
    int error;
};

static inline int rtsp_demuxer_mpegts_onpacket(void* param, int program, int track, int codecid, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes)
{
    static const int s_mpeg2[] = { PSI_STREAM_H264, PSI_STREAM_H265, PSI_STREAM_AAC, PSI_STREAM_MP3, };
    static const int s_payload[] = { RTP_PAYLOAD_H264, RTP_PAYLOAD_H265, RTP_PAYLOAD_MP4A, RTP_PAYLOAD_MP3, };
    static const char* s_encoding[] = {"H264", "H265", "MP4A-LATM", "", };

    int i;
    struct rtsp_demuxer_t* demuxer;
    demuxer = (struct rtsp_demuxer_t*)param;
    
    for(i = 0; i < sizeof(s_mpeg2)/sizeof(s_mpeg2[0]); i++)
    {
        if(codecid == s_mpeg2[i])
            return demuxer->onpacket(demuxer->param, demuxer, track, s_payload[i], s_encoding[i], pts, dts, data, bytes, flags);
    }
    
    return -1;
}

static inline void rtsp_demuxer_mpegps_onpacket(void* param, int track, int codecid, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes)
{
    struct rtsp_demuxer_t* demuxer;
    demuxer = (struct rtsp_demuxer_t*)param;
    demuxer->error = rtsp_demuxer_mpegts_onpacket(param, 0, track, codecid, flags, pts, dts, data, bytes);
    assert(0 == demuxer->error);
}

static inline void rtsp_demuxer_onrtppacket(void* param, const void *data, int bytes, uint32_t timestamp, int flags)
{
    struct rtsp_demuxer_t* demuxer;
    demuxer = (struct rtsp_demuxer_t*)param;
    // FIXME: timestamp round
    // TODO: timestamp
    demuxer->error = demuxer->onpacket(demuxer->param, demuxer, demuxer->stream, demuxer->payload, demuxer->encoding, timestamp, timestamp, data, bytes, flags);
    assert(0 == demuxer->error);
}

static inline void rtsp_demuxer_ontspacket(void* param, const void *packet, int bytes, uint32_t timestamp, int flags)
{
    struct rtsp_demuxer_t* demuxer;
    demuxer = (struct rtsp_demuxer_t*)param;
    demuxer->error = (int)ts_demuxer_input(demuxer->ts, packet, bytes);
    assert(0 == demuxer->error);
}

static inline void rtsp_demuxer_onpspacket(void* param, const void *packet, int bytes, uint32_t timestamp, int flags)
{
    struct rtsp_demuxer_t* demuxer;
    demuxer = (struct rtsp_demuxer_t*)param;
    demuxer->error = (int)ps_demuxer_input(demuxer->ps, packet, bytes);
    assert(0 == demuxer->error);
}

static inline int rtsp_demuxer_create(struct rtsp_demuxer_t* demuxer, int frequency, int payload, const char* encoding, const char* fmtp)
{
    int r;
    demuxer->payload = payload;
    snprintf(demuxer->encoding, sizeof(demuxer->encoding)-1, "%s", encoding);

    if(fmtp && 0 == strcasecmp(encoding, "H264"))
    {
        struct sdp_a_fmtp_h264_t h264;
        r = sdp_a_fmtp_h264(fmtp, &payload, &h264);
    }
    else if(fmtp && (0 == strcasecmp(encoding, "H265") || 0 == strcasecmp(encoding, "HEVC")))
    {
        struct sdp_a_fmtp_h265_t h265;
        r = sdp_a_fmtp_h265(fmtp, &payload, &h265);
    }
    // TODO: h264/h265 vps/sps/pps
    
    if(RTP_PAYLOAD_MP2T == payload)
    {
        demuxer->ts = ts_demuxer_create(rtsp_demuxer_mpegts_onpacket, demuxer);
        demuxer->rtp = rtp_demuxer_create(frequency, payload, encoding, rtsp_demuxer_ontspacket, demuxer);
    }
    else if(0 == strcasecmp(encoding, "MP2P"))
    {
        demuxer->ps = ps_demuxer_create(rtsp_demuxer_mpegps_onpacket, demuxer);
        demuxer->rtp = rtp_demuxer_create(frequency, payload, encoding, rtsp_demuxer_onpspacket, demuxer);
    }
    else
    {
        demuxer->rtp = rtp_demuxer_create(frequency, payload, encoding, rtsp_demuxer_onrtppacket, demuxer);
    }

    return demuxer->rtp ? 0 : -1;
}

static inline int rtsp_demuxer_destroy(struct rtsp_demuxer_t* demuxer)
{
    if(demuxer->ts)
        ts_demuxer_destroy(demuxer->ts);
    if(demuxer->ps)
        ps_demuxer_destroy(demuxer->ps);
    if(demuxer->rtp)
        rtp_demuxer_destroy(&demuxer->rtp);
    return 0;
}

static inline int rtsp_demuxer_input(struct rtsp_demuxer_t* demuxer, const void* data, int bytes)
{
    return rtp_demuxer_input(demuxer->rtp, data, bytes);
}

#endif /* _rtsp_demuxer_h_ */
