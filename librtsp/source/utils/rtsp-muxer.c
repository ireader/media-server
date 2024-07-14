#include "rtsp-muxer.h"
#include "rtp-profile.h"
#include "rtp-payload.h"
#include "rtcp-header.h"
#include "rtp.h"
#include "sdp-a-fmtp.h"
#include "mpeg-ps.h"
#include "mpeg-ts.h"
#include "avbsf.h" // https://github.com/ireader/avcodec
#include "mpeg4-aac.h"
#include "rtp-sender.h"
#include "rtsp-payloads.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <inttypes.h>

#if defined(OS_WINDOWS)
    #if !defined(strcasecmp)
        #define strcasecmp	_stricmp
    #endif
#endif

uint64_t rtpclock(void);

struct rtp_muxer_media_t;
struct rtp_muxer_payload_t;
typedef int (*media_input)(struct rtp_muxer_media_t* m, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes);

struct rtp_muxer_payload_t
{
    struct rtsp_muxer_t* muxer;
    struct rtp_sender_t rtp;
    struct {
        uint32_t rtp; // rtp timestamp
        int64_t pts;
    } timeline; // map a/v pts -> rtp timestamp

    int pid;
    int port;
    uint64_t clock; // rtcp clock
    
    struct ps_muxer_t* ps;
    void* ts; // ts muxer
    int64_t timestamp; // ps/ts only

    const char* sdp;
    int len; // sdp size
};

struct rtp_muxer_media_t
{
	int codec;
    int stream; // ps/ts only
    struct rtp_muxer_payload_t* pt;

    media_input input;

    struct avbsf_t* bsf;
    void* filter;
};

struct rtsp_muxer_t
{
	struct rtp_muxer_payload_t payloads[3];
	int payload_capacity;
	int payload_count;

	struct rtp_muxer_media_t medias[4];
	int media_capacity;
	int media_count;

    rtsp_muxer_onpacket onpacket;
    void* param;

    uint8_t ptr[1024 * 1024];
    int off;
};

static inline uint32_t rtsp_muxer_timeline_update(struct rtp_muxer_payload_t* pt, int64_t pts)
{
    if (0 == pt->timeline.pts)
        pt->timeline.rtp = pt->rtp.timestamp;
    else
        pt->timeline.rtp += (int32_t)(pts - pt->timeline.pts);
    pt->timeline.pts = pts;
    return pt->timeline.rtp;
}

static void* rtsp_muxer_ts_alloc(void* param, size_t bytes)
{
    struct rtp_muxer_payload_t* pt;
    pt = (struct rtp_muxer_payload_t*)param;

    if (bytes > sizeof(pt->muxer->ptr) - pt->muxer->off)
    {
        assert(0);
        return NULL;
    }
    return pt->muxer->ptr + pt->muxer->off;
}

static void rtsp_muxer_ts_free(void* param, void* packet)
{
    (void)param, (void)packet;
}

static int rtsp_muxer_ts_write(void* param, const void* packet, size_t bytes)
{
    struct rtp_muxer_payload_t* pt;
    pt = (struct rtp_muxer_payload_t*)param;
    return rtp_payload_encode_input(pt->rtp.encoder, packet, (int)bytes, rtsp_muxer_timeline_update(pt, pt->timestamp));
}

static int rtsp_muxer_ps_write(void* param, int stream, void* packet, size_t bytes)
{
    struct rtp_muxer_payload_t* pt;
    (void)stream;
    pt = (struct rtp_muxer_payload_t*)param;
    return rtp_payload_encode_input(pt->rtp.encoder, packet, (int)bytes, rtsp_muxer_timeline_update(pt, pt->timestamp));
}

static int rtsp_muxer_rtp_encode_packet(void* param, const void* packet, int bytes, uint32_t timestamp, int flags)
{
    struct rtp_muxer_payload_t* pt;
    pt = (struct rtp_muxer_payload_t*)param;
    return pt->muxer->onpacket(pt->muxer->param, pt->pid, packet, bytes, timestamp, flags);
}

static int rtsp_muxer_ts_input(struct rtp_muxer_media_t* m, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes)
{
    m->pt->timestamp = pts * 90; // last dts
    return mpeg_ts_write(m->pt->ts, m->stream, flags & (MPEG_FLAG_IDR_FRAME | MPEG_FLAG_H264_H265_WITH_AUD) /* ? 0x01 : 0 */, pts * 90, dts * 90, data, bytes);
}

static int rtsp_muxer_ps_input(struct rtp_muxer_media_t* m, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes)
{
    m->pt->timestamp = pts * 90; // last dts
    return ps_muxer_input(m->pt->ps, m->stream, flags & (MPEG_FLAG_IDR_FRAME | MPEG_FLAG_H264_H265_WITH_AUD) /* ? 0x01 : 0 */, pts * 90, dts * 90, data, bytes);
}

static int rtsp_muxer_av_input(struct rtp_muxer_media_t* m, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes)
{
    (void)flags, (void)dts; // TODO: rtp timestamp map PTS
    return rtp_payload_encode_input(m->pt->rtp.encoder, data, (int)bytes, rtsp_muxer_timeline_update(m->pt, pts * m->pt->rtp.frequency / 1000));
}

static int rtsp_muxer_bsf_onpacket(void* param, int64_t pts, int64_t dts, const uint8_t* data, int bytes, int flags)
{
    struct rtp_muxer_media_t* m;
    m = (struct rtp_muxer_media_t*)param;
    return m->input(m, flags, pts, dts, data, bytes);
}

static int rtsp_muxer_payload_close(struct rtp_muxer_payload_t* pt)
{
    if (pt->ps)
    {
        ps_muxer_destroy(pt->ps);
        pt->ps = NULL;
    }

    if (pt->ts)
    {
        mpeg_ts_destroy(pt->ts);
        pt->ts = NULL;
    }

    rtp_sender_destroy(&pt->rtp);
    return 0;
}

struct rtsp_muxer_t* rtsp_muxer_create(rtsp_muxer_onpacket onpacket, void* param)
{
	struct rtsp_muxer_t* muxer;

	muxer = calloc(1, sizeof(*muxer));
	if (!muxer)
		return NULL;

	muxer->payload_capacity = sizeof(muxer->payloads) / sizeof(muxer->payloads[0]);
	muxer->media_capacity = sizeof(muxer->medias) / sizeof(muxer->medias[0]);
    muxer->onpacket = onpacket;
    muxer->param = param;
	return muxer;
}

int rtsp_muxer_destroy(struct rtsp_muxer_t* muxer)
{
    int i;
    struct rtp_muxer_media_t* m;
    struct rtp_muxer_payload_t* pt;

	if (muxer)
	{
        for (i = 0; i < muxer->media_count; i++)
        {
            m = &muxer->medias[i];
            if (m->bsf && m->filter)
                m->bsf->destroy(&m->filter);
        }

        for (i = 0; i < muxer->payload_count; i++)
        {
            pt = &muxer->payloads[i];
            rtsp_muxer_payload_close(pt);
        }

		free(muxer);
	}

	return 0;
}

int rtsp_muxer_add_payload(struct rtsp_muxer_t* muxer, const char* proto, int frequence, int payload, const char* encoding, uint16_t seq, uint32_t ssrc, uint16_t port, const void* extra, int size)
{
    int r = 0;
    struct rtp_muxer_payload_t* pt;

	if (muxer->payload_count >= muxer->payload_capacity)
		return -E2BIG;

	pt = &muxer->payloads[muxer->payload_count];
    memset(pt, 0, sizeof(*pt));
    pt->port = port;
    pt->pid = muxer->payload_count;
    pt->muxer = muxer;
    pt->rtp.seq = seq;
    pt->rtp.ssrc = ssrc;
    pt->rtp.onpacket = rtsp_muxer_rtp_encode_packet;
    pt->rtp.param = pt;

    if (RTP_PAYLOAD_MP2T == payload)
    {
        struct mpeg_ts_func_t h;
        h.alloc = rtsp_muxer_ts_alloc;
        h.write = rtsp_muxer_ts_write;
        h.free = rtsp_muxer_ts_free;

        for (r = 0; r < muxer->payload_count; r++)
        {
            if (muxer->payloads[r].ts)
                return muxer->payloads[r].pid; // exist
        }
        pt->ts = mpeg_ts_create(&h, pt);
        r = rtp_sender_init_video(&pt->rtp, proto, port, payload, encoding, 90000, extra, size);
    }
    else if (0 == strcasecmp(encoding, "MP2P") || 0 == strcasecmp(encoding, "PS"))
    {
        struct ps_muxer_func_t h;
        h.alloc = rtsp_muxer_ts_alloc;
        h.write = rtsp_muxer_ps_write;
        h.free = rtsp_muxer_ts_free;

        for (r = 0; r < muxer->payload_count; r++)
        {
            if (muxer->payloads[r].ps)
                return muxer->payloads[r].pid; // exist
        }
        pt->ps = ps_muxer_create(&h, pt);
        r = rtp_sender_init_video(&pt->rtp, proto, port, payload, encoding, 90000, extra, size);
    }
    else
    {
        if (0 == strcasecmp(encoding, "H264") 
            || 0 == strcasecmp(encoding, "H265") || 0 == strcasecmp(encoding, "HEVC")
            || 0 == strcasecmp(encoding, "VP8") || 0 == strcasecmp(encoding, "VP9")
            || 0 == strcasecmp(encoding, "AV1")
            || 0 == strcasecmp(encoding, "MP4V-ES"))
        {
            r = rtp_sender_init_video(&pt->rtp, proto, port, payload, encoding, frequence ? frequence : 90000, extra, size);
        }
        else if (RTP_PAYLOAD_PCMU == payload || RTP_PAYLOAD_PCMA == payload || 0 == strcasecmp(encoding, "MP4A-LATM") || 0 == strcasecmp(encoding, "MPEG4-GENERIC") || 0 == strcasecmp(encoding, "opus"))
        {
            r = rtp_sender_init_audio(&pt->rtp, proto, port, payload, encoding, frequence, 0 /*from extra*/, extra, size);
        }
        else
        {
            assert(0);
            return -EPROTONOSUPPORT;
        }

    }

    if (r < 0 || r > (int)sizeof(muxer->ptr) - muxer->off)
    {
        rtsp_muxer_payload_close(pt);
        return -1;
    }

    // init timeline
    pt->timeline.rtp = pt->rtp.timestamp;
    pt->timeline.pts = 0;

    // copy sdp
    pt->len = r;
    pt->sdp = (const char*)muxer->ptr + muxer->off;
    memcpy(muxer->ptr + muxer->off, pt->rtp.buffer, r);

    muxer->payload_count++;
    muxer->off += r;
    return pt->pid;
}

int rtsp_muxer_add_media(struct rtsp_muxer_t* muxer, int pid, int codec, const void* extra, int size)
{
    int mpeg2;
    struct rtp_muxer_media_t* m;

    if (muxer->media_count >= muxer->media_capacity)
        return -E2BIG;

    if (pid < 0 || pid >= muxer->payload_count)
        return -ENOENT;

    mpeg2 = avpayload_find_by_payload((uint8_t)codec);
    if (mpeg2 < 0)
        return -EPROTONOSUPPORT;

    m = &muxer->medias[muxer->media_count];
    memset(m, 0, sizeof(*m));
    m->pt = &muxer->payloads[pid];
    m->codec = codec;

    switch (codec)
    {
    case RTP_PAYLOAD_H264:
        //m->bsf = avbsf_find(AVCODEC_VIDEO_H264);
        break;

    case RTP_PAYLOAD_H265:
        //m->bsf = avbsf_find(AVCODEC_VIDEO_H265);
        break;

    case RTP_PAYLOAD_H266:
        //m->bsf = avbsf_find(AVCODEC_VIDEO_H266);
        break;

    case RTP_PAYLOAD_MP4A:
        //m->bsf = avbsf_find(AVCODEC_AUDIO_AAC);
        break;

    default:
        // TODO: opus/g711
        ;
    }

    if (m->bsf)
        m->filter = m->bsf->create(extra, size, rtsp_muxer_bsf_onpacket, m);

    if (m->bsf && !m->filter)
    {
        assert(0);
        return -1;
    }

    if (RTP_PAYLOAD_MP2T == m->pt->rtp.payload)
    {
        m->stream = mpeg_ts_add_stream(m->pt->ts, s_payloads[mpeg2].mpeg2, NULL, 0);
        m->input = rtsp_muxer_ts_input;
    }
    else if (0 == strcasecmp(m->pt->rtp.encoding, "MP2P") || 0 == strcasecmp(m->pt->rtp.encoding, "PS"))
    {
        m->stream = ps_muxer_add_stream(m->pt->ps, s_payloads[mpeg2].mpeg2, NULL, 0);
        m->input = rtsp_muxer_ps_input;
    }
    else
    {
        m->stream = 0; // unuse
        m->input = rtsp_muxer_av_input;
    }

    if (m->stream < 0)
    {
        assert(0);
        return -1;
    }

    return muxer->media_count++;
}

int rtsp_muxer_getinfo(struct rtsp_muxer_t* muxer, int pid, uint16_t* seq, uint32_t* timestamp, const char** sdp, int* size)
{
    struct rtp_muxer_payload_t* pt;
    if (pid < 0 || pid >= muxer->payload_count)
        return -ENOENT;

    pt = &muxer->payloads[pid];
    *sdp = pt->sdp;
    *size = pt->len;
    rtp_payload_encode_getinfo(pt->rtp.encoder, seq, timestamp);
    return 0;
}

int rtsp_muxer_input(struct rtsp_muxer_t* muxer, int mid, int64_t pts, int64_t dts, const void* data, int bytes, int flags)
{
    int r;
    struct rtp_muxer_media_t* m;
    
    if (mid < 0 ||  mid >= muxer->media_count)
        return -ENOENT;

    m = &muxer->medias[mid];

    if (m->bsf && m->filter)
        r = m->bsf->input(m->filter, pts, dts, (const uint8_t*)data, bytes);
    else
        r = m->input(m, flags, pts, dts, data, bytes);

    return r;
}

int rtsp_muxer_rtcp(struct rtsp_muxer_t* muxer, int pid, void* buf, int len)
{
    int r;
    int interval;
    uint64_t clock;
    struct rtp_muxer_payload_t* pt;

    if (pid < 0 || pid >= muxer->payload_count)
        return -ENOENT;
    pt = &muxer->payloads[pid];

    r = 0;
    clock = rtpclock();
    interval = rtp_rtcp_interval(pt->rtp.rtp);
    if (pt->clock + (uint64_t)interval * 1000 < clock)
    {
        // RTCP report
        r = rtp_rtcp_report(pt->rtp.rtp, buf, len);
        pt->clock = clock;
    }

    return r;
}

int rtsp_muxer_onrtcp(struct rtsp_muxer_t* muxer, int pid, const void* buf, int len)
{
    struct rtp_muxer_payload_t* pt;
    if (pid < 0 || pid >= muxer->payload_count)
        return -ENOENT;
    pt = &muxer->payloads[pid];
    return rtp_onreceived_rtcp(pt->rtp.rtp, buf, len);
}
