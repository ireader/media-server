#include "rtsp-demuxer.h"
#include "rtp-demuxer.h"
#include "rtp-profile.h"
#include "rtcp-header.h"
#include "sdp-a-fmtp.h"
#include "mpeg-ts-proto.h"
#include "mpeg-ps.h"
#include "mpeg-ts.h"
#include "avpbs.h" // https://github.com/ireader/avcodec #avbsf/include
#include "mpeg4-aac.h"
#include "rtsp-payloads.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>

#define RTSP_STREAM_MAX         4
#define RTSP_PAYLOAD_MAX        8
#define RTSP_PAYLOAD_MAX_SIZE   (10 * 1024 * 1024)

#if defined(OS_WINDOWS)
    #if !defined(strcasecmp)
        #define strcasecmp	_stricmp
    #endif
#endif

struct rtsp_demuxer_t;
struct rtp_payload_info_t
{
    struct rtsp_demuxer_t* ctx;

    int frequency;
    int payload;
    char encoding[64];

    uint8_t* extra;
    int extra_bytes;

    uint16_t seq; // RTP-Info seq
    uint32_t base; // RTP-Info timestamp

    uint32_t last; // last rtp packet timestamp
    int64_t pts; // last mapped rtp packet timestamp

    struct rtp_demuxer_t* rtp;
    void* ts, * ps; // only one

    union
    {
        struct sdp_a_fmtp_h264_t h264;
        struct sdp_a_fmtp_h265_t h265;
        struct sdp_a_fmtp_mpeg4_t mpeg4;

        struct mpeg4_aac_t aac;
    } fmtp;

    struct avpbs_t* bs;
    void* filter;

    // ps/ts only
    struct
    {
        AVPACKET_CODEC_ID codec;
        struct avpbs_t* bs;
        void* filter;
        int pid; // ts/ps only
    } tracks[RTSP_STREAM_MAX];

    // ps only
    struct {
        uint8_t* ptr;
        int len, cap;
    } ptr;
};

struct rtsp_demuxer_t
{
    void* param;
    rtsp_demuxer_onpacket onpacket;

    int jitter;
    struct rtp_payload_info_t pt[RTSP_PAYLOAD_MAX];
    int count; // payload type count
    int idx; // last payload index

    uint8_t ptr[8 * 1024];
    int off;
};

int sdp_aac_latm_load(uint8_t* data, int bytes, const char* config);
int sdp_aac_mpeg4_load(uint8_t* data, int bytes, const char* config);
int sdp_h264_load(uint8_t* data, int bytes, const char* config);
int sdp_h265_load(uint8_t* data, int bytes, const char* vps, const char* sps, const char* pps, const char* sei);

static int rtsp_demuxer_avpbs_onpacket(void* param, struct avpacket_t* pkt)
{
    struct rtp_payload_info_t* pt;
    pt = (struct rtp_payload_info_t*)param;
    return pt->ctx->onpacket(pt->ctx->param, pkt);
}

static int rtsp_demuxer_mpeg2_fetch_stream(struct rtp_payload_info_t* pt, int pid, int codecid)
{
    int i, mpeg2;

    mpeg2 = avpayload_find_by_mpeg2((uint8_t)codecid);
    if (mpeg2 < 0)
        return -1;

    for (i = 0; i < sizeof(pt->tracks) / sizeof(pt->tracks[0]); i++)
    {
        if (0 == pt->tracks[i].pid || pt->tracks[i].pid == pid)
            break;
    }

    if (i >= sizeof(pt->tracks) / sizeof(pt->tracks[0]))
        return -1;

    if(pt->tracks[i].codec != s_payloads[mpeg2].codecid && pt->tracks[i].bs && pt->tracks[i].filter)
        pt->tracks[i].bs->destroy(&pt->tracks[i].filter);
        
    if (!pt->tracks[i].bs || !pt->tracks[i].filter)
    {
        pt->tracks[i].codec = s_payloads[mpeg2].codecid;
        pt->tracks[i].bs = avpbs_find(s_payloads[mpeg2].codecid);
        if(pt->tracks[i].bs)
            pt->tracks[i].filter = pt->tracks[i].bs->create(i, s_payloads[mpeg2].codecid, NULL, 0, rtsp_demuxer_avpbs_onpacket, pt);
    }

    pt->tracks[i].pid = pid;
    return pt->tracks[i].filter ? i : -1;
}

static inline int rtsp_demuxer_mpegts_onpacket(void* param, int program, int track, int codecid, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes)
{
    int i, r, len;
    struct rtp_payload_info_t* pt;
    pt = (struct rtp_payload_info_t*)param;

    i = rtsp_demuxer_mpeg2_fetch_stream(pt, track, codecid);
    if (i >= 0)
    {
        flags = flags ? AVPACKET_FLAG_KEY : 0;

        if (PSI_STREAM_AAC == codecid)
        {
            if (0 == pt->fmtp.aac.sampling_frequency)
                mpeg4_aac_adts_load((const uint8_t*)data, bytes, &pt->fmtp.aac);

            len = mpeg4_aac_adts_frame_length((const uint8_t*)data, bytes);
            while (pt->fmtp.aac.sampling_frequency > 0 && len > 7 && (size_t)len <= bytes)
            {
                r = pt->tracks[i].bs->input(pt->tracks[i].filter, pts / 90, dts / 90, (const uint8_t*)data, bytes, 0);
                if (0 != r)
                    return r;

                pts += 90000 /* 90KHz */ * 1024 /*samples per frame*/ / pt->fmtp.aac.sampling_frequency;
                dts += 90000 /* 90KHz */ * 1024 /*samples per frame*/ / pt->fmtp.aac.sampling_frequency;
                bytes -= len;
                data = (const uint8_t*)data + len;
                len = mpeg4_aac_adts_frame_length((const uint8_t*)data, bytes);
            }

            return bytes > 0 ? pt->tracks[i].bs->input(pt->tracks[i].filter, pts / 90, dts / 90, (const uint8_t*)data, bytes, 0) : 0;
        }
        else
        {
            return pt->tracks[i].bs->input(pt->tracks[i].filter, pts / 90, dts / 90, (const uint8_t*)data, bytes, flags);
        }
    }

    if (0xbd == codecid)
        return 0; // ignore HIK private stream

    (void)program; //ignore
    assert(0);
    return -1; // discard
}

static inline int rtsp_demuxer_mpegps_onpacket(void* param, int track, int codecid, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes)
{
    //struct rtp_payload_info_t* pt;
    //pt = (struct rtp_payload_info_t*)param;
    (void)param;
    return rtsp_demuxer_mpegts_onpacket(param, 0, track, codecid, flags, pts, dts, data, bytes);
}

static inline int rtsp_demuxer_ontspacket(void* param, const void* packet, int bytes, uint32_t timestamp, int flags)
{
    int r;
    struct rtp_payload_info_t* pt;
    pt = (struct rtp_payload_info_t*)param;
    
    r = 0;
    while (0 == r && bytes >= 188)
    {
        r = (int)ts_demuxer_input(pt->ts, packet, 188);
        assert(0 == r);
        bytes -= 188;
        packet = (const uint8_t*)packet + 188;
    }
    (void)timestamp, (void)flags; //ignore

    return r;
}

static inline int rtsp_demuxer_merge_ps_buffer(struct rtp_payload_info_t* pt, const uint8_t* packet, int bytes)
{
    int n;
    uint8_t* ptr;
    if (bytes < 0)
        return -EINVAL;

    if (pt->ptr.len + bytes > pt->ptr.cap)
    {
        if (pt->ptr.len + bytes > RTSP_PAYLOAD_MAX_SIZE)
        {
            pt->ptr.len = 0; // clear
            return -E2BIG;
        }

        n = pt->ptr.len + bytes + 64 * 1024;
        ptr = (uint8_t*)realloc(pt->ptr.ptr, n);
        if (!ptr)
        {
            pt->ptr.len = 0; // clear
            return -ENOMEM;
        }

        pt->ptr.ptr = ptr;
        pt->ptr.cap = n;
    }

    if (bytes > 0)
    {
        memmove(pt->ptr.ptr + pt->ptr.len, packet, bytes);
        pt->ptr.len += bytes;
    }

    return 0;
}

static inline int rtsp_demuxer_onpspacket(void* param, const void* packet, int bytes, uint32_t timestamp, int flags)
{
    int r, n;
    struct rtp_payload_info_t* pt;
    pt = (struct rtp_payload_info_t*)param;
    if (pt->ptr.len > 0)
    {
        r = rtsp_demuxer_merge_ps_buffer(pt, packet, bytes);
        if (0 != r)
            return r;

        packet = pt->ptr.ptr;
        bytes = pt->ptr.len;
        pt->ptr.len = 0;
    }

    n = (int)ps_demuxer_input(pt->ps, packet, bytes);
    assert(n <= bytes);
    if (n >= 0 && n < bytes)
        r = rtsp_demuxer_merge_ps_buffer(pt, (const uint8_t*)packet + (bytes - n), bytes - n);
    else
        r = n < 0 ? n : 0;

    (void)timestamp, (void)flags; //ignore
    return r;
}

static inline int rtsp_demuxer_onrtppacket(void* param, const void* data, int bytes, uint32_t timestamp, int flags)
{
    int r;
    struct rtp_payload_info_t* pt;
    pt = (struct rtp_payload_info_t*)param;

    // RTP timestamp => PTS/DTS
    if (0 == pt->last && 0 == pt->pts)
    {
        pt->last = timestamp;
        pt->pts = 0;
    }
    else
    {
        pt->pts += ((int64_t)(int32_t)(timestamp - pt->last)) * 1000 / pt->frequency;
        pt->last = timestamp;
    }

    r = -1;
    if (pt->bs && pt->filter)
    {
        //flags = 0;
        r = pt->bs->input(pt->filter, pt->pts, pt->pts, (const uint8_t*)data, bytes, flags);
    }
    //else
    //    r = pt->ctx->onpacket(pt->ctx->param, 0, pt->payload, pt->encoding, pt->pts, pt->pts, data, bytes, flags);
    
    return r;
}

static int rtsp_demuxer_payload_close(struct rtp_payload_info_t* pt)
{
    if (pt->ptr.ptr)
    {
        assert(pt->ptr.cap > 0);
        free(pt->ptr.ptr);
        pt->ptr.ptr = NULL;
    }

    if (pt->bs && pt->filter)
    {
        pt->bs->destroy(&pt->filter);
        pt->bs = NULL;
    }

    if (pt->ts)
    {
        ts_demuxer_destroy(pt->ts);
        pt->ts = NULL;
    }

    if (pt->ps)
    {
        ps_demuxer_destroy(pt->ps);
        pt->ps = NULL;
    }

    if (pt->rtp)
    {
        rtp_demuxer_destroy(&pt->rtp);
        pt->rtp = NULL;
    }

    return 0;
}

int rtsp_demuxer_add_payload(struct rtsp_demuxer_t* demuxer, int frequency, int payload, const char* encoding, const char* fmtp)
{
    int r, len;
    AVPACKET_CODEC_ID codec;
    struct rtp_payload_info_t* pt;
    if (demuxer->count >= sizeof(demuxer->pt) / sizeof(demuxer->pt[0]))
        return -1; // too many payload type

    assert(frequency > 0);
    pt = &demuxer->pt[demuxer->count];
    memset(pt, 0, sizeof(*pt));
    snprintf(pt->encoding, sizeof(pt->encoding) - 1, "%s", encoding);
    pt->frequency = frequency;
    pt->payload = payload;
    pt->extra = demuxer->ptr + demuxer->off;
    pt->extra_bytes = 0;
    pt->ctx = demuxer;

    if (RTP_PAYLOAD_MP2T == payload)
    {
        pt->ts = ts_demuxer_create(rtsp_demuxer_mpegts_onpacket, pt);
        pt->rtp = rtp_demuxer_create(demuxer->jitter, frequency, payload, encoding, rtsp_demuxer_ontspacket, pt);
    }
    else if (0 == strcasecmp(encoding, "MP2P") || 0 == strcasecmp(encoding, "PS"))
    {
        pt->ps = ps_demuxer_create(rtsp_demuxer_mpegps_onpacket, pt);
        pt->rtp = rtp_demuxer_create(demuxer->jitter, frequency, payload, encoding, rtsp_demuxer_onpspacket, pt);
    }
    else
    {
        assert(demuxer->off <= sizeof(demuxer->ptr));
        len = sizeof(demuxer->ptr) - demuxer->off;
        r = avpayload_find_by_rtp(payload, encoding);
        codec = -1 != r ? s_payloads[r].codecid : AVCODEC_NONE;
        switch (codec)
        {
        case AVCODEC_VIDEO_H264:
            if (fmtp && *fmtp && 0 == sdp_a_fmtp_h264(fmtp, &payload, &pt->fmtp.h264))
                pt->extra_bytes = sdp_h264_load(pt->extra, len, pt->fmtp.h264.sprop_parameter_sets);
            pt->bs = avpbs_find(AVCODEC_VIDEO_H264);
            break;

        case AVCODEC_VIDEO_H265:
            if (fmtp && *fmtp && 0 == sdp_a_fmtp_h265(fmtp, &payload, &pt->fmtp.h265))
                pt->extra_bytes = sdp_h265_load(pt->extra, len, pt->fmtp.h265.sprop_vps, pt->fmtp.h265.sprop_sps, pt->fmtp.h265.sprop_pps, pt->fmtp.h265.sprop_sei);
            pt->bs = avpbs_find(AVCODEC_VIDEO_H265);
            break;

        case AVCODEC_AUDIO_AAC:
            if (0 == strcasecmp(encoding, "MPEG4-GENERIC"))
            {
                if (fmtp && *fmtp && 0 == sdp_a_fmtp_mpeg4(fmtp, &payload, &pt->fmtp.mpeg4))
                    pt->extra_bytes = sdp_aac_mpeg4_load(pt->extra, len, pt->fmtp.mpeg4.config);
                pt->bs = avpbs_find(AVCODEC_AUDIO_AAC);
            }
            else if (0 == strcasecmp(encoding, "MP4A-LATM"))
            {
                if (fmtp && *fmtp && 0 == sdp_a_fmtp_mpeg4(fmtp, &payload, &pt->fmtp.mpeg4))
                    pt->extra_bytes = sdp_aac_latm_load(pt->extra, len, pt->fmtp.mpeg4.config);
                pt->bs = avpbs_find(AVCODEC_AUDIO_AAC);
            }
            else
            {
                pt->bs = avpbs_find(AVCODEC_AUDIO_AAC);
            }

        // TODO:
        case AVCODEC_VIDEO_AV1:
        case AVCODEC_VIDEO_VP8:
        case AVCODEC_VIDEO_VP9:
        case AVCODEC_AUDIO_MP3:
        case AVCODEC_AUDIO_OPUS:
        default:
            pt->bs = avpbs_find(codec); // common
        }

        if (pt->bs)
            pt->filter = pt->bs->create(demuxer->count, codec, pt->extra, pt->extra_bytes > 0 ? pt->extra_bytes : 0, rtsp_demuxer_avpbs_onpacket, pt);
        pt->rtp = rtp_demuxer_create(demuxer->jitter, frequency, pt->payload, encoding, rtsp_demuxer_onrtppacket, pt);
    }

    if (!pt->rtp || (pt->bs && !pt->filter))
    {
        rtsp_demuxer_payload_close(pt);
        return -1;
    }

    demuxer->count++;
    demuxer->off += pt->extra_bytes;
    assert(demuxer->off <= sizeof(demuxer->ptr));
    return 0;
}

struct rtsp_demuxer_t* rtsp_demuxer_create(int jitter, rtsp_demuxer_onpacket onpkt, void* param)
{
    struct rtsp_demuxer_t* demuxer;
    demuxer = calloc(1, sizeof(*demuxer));
    if (!demuxer) return NULL;

    demuxer->jitter = jitter;
    demuxer->param = param;
    demuxer->onpacket = onpkt;
    return demuxer;
}

int rtsp_demuxer_destroy(struct rtsp_demuxer_t* demuxer)
{
    int i;
    if (demuxer)
    {
        for (i = 0; i < demuxer->count; i++)
            rtsp_demuxer_payload_close(&demuxer->pt[i]);
        free(demuxer);
    }
    return 0;
}

int rtsp_demuxer_rtpinfo(struct rtsp_demuxer_t* demuxer, uint16_t seq, uint32_t timestamp)
{
    struct rtp_payload_info_t* pt;
    if (demuxer->idx >= demuxer->count || demuxer->idx < 0)
    {
        assert(0);
        return -1;
    }

    pt = &demuxer->pt[demuxer->idx];
    pt->seq = seq;
    pt->base = timestamp;
    if (0 == pt->last)
        pt->last = timestamp;
    pt->pts = ((int64_t)(int32_t)(pt->last - timestamp)) * 1000 / pt->frequency;
    return 0;
}

int rtsp_demuxer_input(struct rtsp_demuxer_t* demuxer, const void* data, int bytes)
{
    int i;
    uint8_t id;
    struct rtp_payload_info_t* pt;

    id = bytes > 1 ? ((const uint8_t*)data)[1] : 255;
    for (i = demuxer->idx; i < demuxer->count + demuxer->idx; i++)
    {
        pt = &demuxer->pt[i % demuxer->count];

        // RFC7983 SRTP: https://tools.ietf.org/html/draft-ietf-avtcore-rfc5764-mux-fixes
        assert(((const uint8_t*)data)[0] > 127 && ((const uint8_t*)data)[0] < 192);

        // http://www.iana.org/assignments/rtp-parameters/rtp-parameters.xhtml#rtp-parameters-4
        // RFC 5761 (RTCP-mux) states this range for secure RTCP/RTP detection.
        // RTCP packet types in the ranges 1-191 and 224-254 SHOULD only be used when other values have been exhausted.
        if ( (id & 0x7F) == pt->payload || (192 <= id && id <= 223) )
        {
            demuxer->idx = i % demuxer->count;
            return rtp_demuxer_input(pt->rtp, data, bytes);
        }
    }

    //assert(0);
    //return -1;
    return 0;
}

int rtsp_demuxer_rtcp(struct rtsp_demuxer_t* demuxer, void* buf, int len)
{
    if (demuxer->idx >= demuxer->count || demuxer->idx < 0)
    {
        assert(0);
        return -1;
    }

    return rtp_demuxer_rtcp(demuxer->pt[demuxer->idx].rtp, buf, len);
}
