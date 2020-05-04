#include "rtsp-demuxer.h"
#include "rtp-demuxer.h"
#include "rtp-profile.h"
#include "sdp-a-fmtp.h"
#include "mpeg-ts-proto.h"
#include "mpeg-ps.h"
#include "mpeg-ts.h"
#include "avbsf.h" // https://github.com/ireader/avcodec
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#if defined(OS_WINDOWS)
    #if !defined(strcasecmp)
        #define strcasecmp	_stricmp
    #endif
#endif

struct rtsp_demuxer_t
{
    void* param;
    int (*onpacket)(void* param, int stream, int payload, const char* encoding, int64_t pts, int64_t dts, const void* data, int bytes, int flags);

    struct rtp_demuxer_t* rtp;
    void *ts, *ps; // only one

    int frequency;
    int payload;
    char encoding[64];
    uint16_t seq; // RTP-Info seq
    uint32_t base; // RTP-Info timestamp
    
    uint32_t last; // last rtp packet timestamp
    int64_t pts; // last mapped rtp packet timestamp

    union
    {
        struct sdp_a_fmtp_h264_t h264;
        struct sdp_a_fmtp_h265_t h265;
        struct sdp_a_fmtp_mpeg4_t mpeg4;
    } fmtp;

    struct avbsf_t* bsf;
    void* filter;

    uint8_t extra[4 * 1024];
    int extra_bytes;
    int error;
};

int sdp_aac_latm_load(uint8_t* data, int bytes, const char* config);
int sdp_aac_mpeg4_load(uint8_t* data, int bytes, const char* config);
int sdp_h264_load(uint8_t* data, int bytes, const char* config);
int sdp_h265_load(uint8_t* data, int bytes, const char* vps, const char* sps, const char* pps, const char* sei);

static inline int rtsp_demuxer_mpegts_onpacket(void* param, int program, int track, int codecid, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes)
{
    static const int s_mpeg2[] = { PSI_STREAM_H264, PSI_STREAM_H265, PSI_STREAM_AAC, PSI_STREAM_MP3, };
    static const int s_payload[] = { RTP_PAYLOAD_H264, RTP_PAYLOAD_H265, RTP_PAYLOAD_MP4A, RTP_PAYLOAD_MP3, };
    static const char* s_encoding[] = { "H264", "H265", "MP4A-LATM", "", };

    int i;
    struct rtsp_demuxer_t* demuxer;
    demuxer = (struct rtsp_demuxer_t*)param;

    for (i = 0; i < sizeof(s_mpeg2) / sizeof(s_mpeg2[0]); i++)
    {
        if (codecid == s_mpeg2[i])
            return demuxer->onpacket(demuxer->param, track, s_payload[i], s_encoding[i], pts, dts, data, bytes, flags);
    }

    (void)program; //ignore
    return -1;
}

static inline void rtsp_demuxer_mpegps_onpacket(void* param, int track, int codecid, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes)
{
    struct rtsp_demuxer_t* demuxer;
    demuxer = (struct rtsp_demuxer_t*)param;
    demuxer->error = rtsp_demuxer_mpegts_onpacket(param, 0, track, codecid, flags, pts, dts, data, bytes);
    assert(0 == demuxer->error);
}

static int rtsp_demuxer_bsf_onpacket(void* param, int64_t pts, int64_t dts, const void* data, int bytes, int flags)
{
    struct rtsp_demuxer_t* demuxer;
    demuxer = (struct rtsp_demuxer_t*)param;
    return demuxer->onpacket(demuxer->param, 0, demuxer->payload, demuxer->encoding, pts, dts, data, bytes, flags);
}

static inline void rtsp_demuxer_ontspacket(void* param, const void* packet, int bytes, uint32_t timestamp, int flags)
{
    struct rtsp_demuxer_t* demuxer;
    demuxer = (struct rtsp_demuxer_t*)param;
    demuxer->error = (int)ts_demuxer_input(demuxer->ts, packet, bytes);
    assert(0 == demuxer->error);
    (void)timestamp, (void)flags; //ignore
}

static inline void rtsp_demuxer_onpspacket(void* param, const void* packet, int bytes, uint32_t timestamp, int flags)
{
    struct rtsp_demuxer_t* demuxer;
    demuxer = (struct rtsp_demuxer_t*)param;
    demuxer->error = (int)ps_demuxer_input(demuxer->ps, packet, bytes);
    assert(0 == demuxer->error);
    (void)timestamp, (void)flags; //ignore
}

static inline void rtsp_demuxer_onrtppacket(void* param, const void* data, int bytes, uint32_t timestamp, int flags)
{
    struct rtsp_demuxer_t* demuxer;
    demuxer = (struct rtsp_demuxer_t*)param;

    // RTP timestamp => PTS/DTS
    if (0 == demuxer->last && 0 == demuxer->pts)
    {
        demuxer->last = timestamp;
        demuxer->pts = 0;
    }
    else
    {
        demuxer->pts += ((int64_t)(int32_t)(timestamp - demuxer->last)) * 1000 / demuxer->frequency;
        demuxer->last = timestamp;
    }

    if (demuxer->bsf && demuxer->filter)
        demuxer->error = demuxer->bsf->input(demuxer->filter, demuxer->pts, demuxer->pts, (const uint8_t*)data, bytes);
    else
        demuxer->error = demuxer->onpacket(demuxer->param, 0, demuxer->payload, demuxer->encoding, demuxer->pts, demuxer->pts, data, bytes, flags);
    assert(0 == demuxer->error);
}

struct rtsp_demuxer_t* rtsp_demuxer_create(int frequency, int payload, const char* encoding, const char* fmtp, rtsp_demuxer_onpacket onpkt, void* param)
{
    struct rtsp_demuxer_t* demuxer;
    demuxer = calloc(1, sizeof(*demuxer));
    if (!demuxer) return NULL;

    assert(frequency > 0);
    demuxer->param = param;
    demuxer->onpacket = onpkt;
    demuxer->frequency = frequency;
    demuxer->payload = payload;
    snprintf(demuxer->encoding, sizeof(demuxer->encoding) - 1, "%s", encoding);

    if (RTP_PAYLOAD_MP2T == payload)
    {
        demuxer->ts = ts_demuxer_create(rtsp_demuxer_mpegts_onpacket, demuxer);
        demuxer->rtp = rtp_demuxer_create(frequency, payload, encoding, rtsp_demuxer_ontspacket, demuxer);
    }
    else if (0 == strcasecmp(encoding, "MP2P"))
    {
        demuxer->ps = ps_demuxer_create(rtsp_demuxer_mpegps_onpacket, demuxer);
        demuxer->rtp = rtp_demuxer_create(frequency, payload, encoding, rtsp_demuxer_onpspacket, demuxer);
    }
    else
    {
        if (0 == strcasecmp(encoding, "H264"))
        {
            if (fmtp && 0 == sdp_a_fmtp_h264(fmtp, &payload, &demuxer->fmtp.h264))
                demuxer->extra_bytes = sdp_h264_load(demuxer->extra, sizeof(demuxer->extra), demuxer->fmtp.h264.sprop_parameter_sets);
            demuxer->bsf = avbsf_h264();
        }
        else if (0 == strcasecmp(encoding, "H265") || 0 == strcasecmp(encoding, "HEVC"))
        {
            if (fmtp && 0 == sdp_a_fmtp_h265(fmtp, &payload, &demuxer->fmtp.h265))
                demuxer->extra_bytes = sdp_h265_load(demuxer->extra, sizeof(demuxer->extra), demuxer->fmtp.h265.sprop_vps, demuxer->fmtp.h265.sprop_sps, demuxer->fmtp.h265.sprop_pps, demuxer->fmtp.h265.sprop_sei);
            demuxer->bsf = avbsf_h265();
        }
        else if (fmtp && (0 == strcasecmp(encoding, "MPEG4-GENERIC")))
        {
            if (fmtp && 0 == sdp_a_fmtp_mpeg4(fmtp, &payload, &demuxer->fmtp.mpeg4))
                demuxer->extra_bytes = sdp_aac_mpeg4_load(demuxer->extra, sizeof(demuxer->extra), demuxer->fmtp.mpeg4.config);
            demuxer->bsf = avbsf_aac();
        }
        else if (fmtp && (0 == strcasecmp(encoding, "MP4A-LATM")))
        {
            if (fmtp && 0 == sdp_a_fmtp_mpeg4(fmtp, &payload, &demuxer->fmtp.mpeg4))
                demuxer->extra_bytes = sdp_aac_latm_load(demuxer->extra, sizeof(demuxer->extra), demuxer->fmtp.mpeg4.config);
            demuxer->bsf = avbsf_aac();
        }

        if(demuxer->bsf)
            demuxer->filter = demuxer->bsf->create(demuxer->extra, demuxer->extra_bytes > 0 ? demuxer->extra_bytes : 0, rtsp_demuxer_bsf_onpacket, demuxer);
        demuxer->rtp = rtp_demuxer_create(frequency, payload, encoding, rtsp_demuxer_onrtppacket, demuxer);
    }

    if (!demuxer->rtp || (demuxer->bsf && !demuxer->filter))
    {
        rtsp_demuxer_destroy(demuxer);
        return NULL;
    }

    return demuxer;
}

int rtsp_demuxer_destroy(struct rtsp_demuxer_t* demuxer)
{
    if (demuxer->bsf && demuxer->filter)
    {
        demuxer->bsf->destroy(&demuxer->filter);
        demuxer->bsf = NULL;
    }

    if (demuxer->ts)
        ts_demuxer_destroy(demuxer->ts);
    if (demuxer->ps)
        ps_demuxer_destroy(demuxer->ps);
    if (demuxer->rtp)
        rtp_demuxer_destroy(&demuxer->rtp);
    return 0;
}

int rtsp_demuxer_rtpinfo(struct rtsp_demuxer_t* demuxer, uint16_t seq, uint32_t timestamp)
{
    demuxer->seq = seq;
    demuxer->base = timestamp;
    if (0 == demuxer->last)
        demuxer->last = timestamp;
    demuxer->pts = ((int64_t)(int32_t)(demuxer->last - timestamp)) * 1000 / demuxer->frequency;
    return 0;
}

int rtsp_demuxer_input(struct rtsp_demuxer_t* demuxer, const void* data, int bytes)
{
    return rtp_demuxer_input(demuxer->rtp, data, bytes);
}

int rtsp_demuxer_rtcp(struct rtsp_demuxer_t* demuxer, void* buf, int len)
{
    return rtp_demuxer_rtcp(demuxer->rtp, buf, len);
}
