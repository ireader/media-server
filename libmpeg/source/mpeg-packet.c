#include "mpeg-pes-internal.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#define MPEG_PACKET_PAYLOAD_MAX_SIZE (10 * 1024 * 1024)

typedef int (*h2645_find_new_access)(const uint8_t* p, size_t bytes, int* vcl);

static int mpeg_packet_is_mpeg12_video(int codecid)
{
    return PSI_STREAM_MPEG1 == codecid || PSI_STREAM_MPEG2 == codecid;
}

static int mpeg_packet_append(struct packet_t* pkt, const void* data, size_t size)
{
    void* ptr;

    if (0 == size)
        return 0;
    if (NULL == data)
        return -EINVAL;

    // fix: pkt->size + size bits wrap
    if (pkt->size + size > MPEG_PACKET_PAYLOAD_MAX_SIZE || pkt->size + size < pkt->size)
        return -EINVAL;

    if (pkt->capacity < pkt->size + size)
    {
        ptr = realloc(pkt->data, pkt->size + size + 2048);
        if (NULL == ptr) return -ENOMEM;
        pkt->data = (uint8_t*)ptr;
        pkt->capacity = pkt->size + size + 2048;
    }

    // append new data
    memcpy(pkt->data + pkt->size, data, size);
    pkt->size += size;
    return 0;
}

static void mpeg_packet_mpeg12_reset(struct packet_t* pkt)
{
    pkt->size = 0;
    pkt->flags = 0;
    pkt->mpeg12_scan = 0;
    pkt->mpeg12_prefix = 0;
    pkt->mpeg12_picture = 0;
    pkt->mpeg12_prefix_flags = 0;
    pkt->mpeg12_picture_flags = 0;
    pkt->mpeg12_picture_pts = 0;
    pkt->mpeg12_picture_dts = 0;
    pkt->mpeg12_have_prefix = 0;
    pkt->mpeg12_have_picture = 0;
    pkt->mpeg12_have_picture_timestamp = 0;
    pkt->mpeg12_tail_count = 0;
    pkt->mpeg12_retry = 0;
    pkt->mpeg12_retry_flush = 0;
}

static void mpeg_packet_mpeg12_save_retry(struct packet_t* pkt, const struct pes_t* pes, int flush)
{
    pkt->mpeg12_retry_pn = pes->pn;
    pkt->mpeg12_retry_pid = pes->pid;
    pkt->mpeg12_retry_sid = pes->sid;
    pkt->mpeg12_retry_codecid = pes->codecid;
    pkt->mpeg12_retry_flags = pes->flags;
    pkt->mpeg12_retry_pts = pes->pts;
    pkt->mpeg12_retry_dts = pes->dts;
    pkt->mpeg12_retry_flush = (uint8_t)(0 != flush);
    pkt->mpeg12_retry = 1;
}

static void mpeg_packet_mpeg12_compact(struct packet_t* pkt, const struct pes_t* pes,
    size_t old_size, size_t old_tail_start, const int64_t old_tail_pts[3],
    const int64_t old_tail_dts[3], const int old_tail_flags[3], int flags,
    size_t keep, size_t scan)
{
    size_t i;
    size_t new_tail_count;
    size_t new_tail_start;
    int64_t new_tail_pts[3];
    int64_t new_tail_dts[3];
    int new_tail_flags[3];

    assert(keep <= pkt->size);
    new_tail_count = pkt->size - keep < 3 ? pkt->size - keep : 3;
    new_tail_start = pkt->size - new_tail_count;
    for (i = 0; i < new_tail_count; ++i)
    {
        size_t offset = new_tail_start + i;
        if (offset < old_size)
        {
            assert(offset >= old_tail_start);
            new_tail_pts[i] = old_tail_pts[offset - old_tail_start];
            new_tail_dts[i] = old_tail_dts[offset - old_tail_start];
            new_tail_flags[i] = old_tail_flags[offset - old_tail_start];
        }
        else
        {
            new_tail_pts[i] = pes->pts;
            new_tail_dts[i] = pes->dts;
            new_tail_flags[i] = flags;
        }
    }

    if (keep > 0)
    {
        memmove(pkt->data, pkt->data + keep, pkt->size - keep);
        pkt->size -= keep;
        scan = scan > keep ? scan - keep : 0;
        if (pkt->mpeg12_have_prefix)
        {
            assert(pkt->mpeg12_prefix >= keep);
            pkt->mpeg12_prefix -= keep;
        }
        if (pkt->mpeg12_have_picture_timestamp)
        {
            assert(pkt->mpeg12_picture >= keep);
            pkt->mpeg12_picture -= keep;
        }
    }

    pkt->mpeg12_scan = scan;
    pkt->mpeg12_tail_count = (uint8_t)new_tail_count;
    memcpy(pkt->mpeg12_tail_pts, new_tail_pts, new_tail_count * sizeof(new_tail_pts[0]));
    memcpy(pkt->mpeg12_tail_dts, new_tail_dts, new_tail_count * sizeof(new_tail_dts[0]));
    memcpy(pkt->mpeg12_tail_flags, new_tail_flags, new_tail_count * sizeof(new_tail_flags[0]));
}

static int mpeg_packet_mpeg12(struct packet_t* pkt, const struct pes_t* pes, size_t appended, int flush, pes_packet_handler handler, void* param)
{
    int r;
    int flags;
    int start_flags;
    size_t i;
    size_t j;
    size_t keep;
    size_t scan;
    size_t boundary;
    size_t old_size;
    size_t old_tail_start;
    uint8_t code;
    uint8_t picture_type;
    uint8_t old_tail_count;
    int64_t picture_pts;
    int64_t picture_dts;
    int64_t old_tail_pts[3];
    int64_t old_tail_dts[3];
    int old_tail_flags[3];

    keep = 0;
    old_size = pkt->size - appended;
    old_tail_count = pkt->mpeg12_tail_count;
    // A start code can begin in the last three bytes of the previous PES input.
    // Keep their metadata so the new Picture retains its original timestamp.
    memcpy(old_tail_pts, pkt->mpeg12_tail_pts, sizeof(old_tail_pts));
    memcpy(old_tail_dts, pkt->mpeg12_tail_dts, sizeof(old_tail_dts));
    memcpy(old_tail_flags, pkt->mpeg12_tail_flags, sizeof(old_tail_flags));
    old_tail_start = old_size - old_tail_count;
    flags = pes->flags & (MPEG_FLAG_PACKET_LOST | MPEG_FLAG_PACKET_CORRUPT);
    if (pkt->mpeg12_have_picture)
        pkt->flags |= flags;
    if (pkt->mpeg12_have_prefix)
        pkt->mpeg12_prefix_flags |= flags;

    for (i = pkt->mpeg12_scan; i + 3 < pkt->size; ++i)
    {
        if (0x00 != pkt->data[i] || 0x00 != pkt->data[i + 1] || 0x01 != pkt->data[i + 2])
            continue;

        code = pkt->data[i + 3];
        if (i < old_size && i >= old_tail_start)
            start_flags = old_tail_flags[i - old_tail_start];
        else
            start_flags = flags;

        if (0xB3 == code || 0xB8 == code)
        {
            if (!pkt->mpeg12_have_prefix)
            {
                pkt->mpeg12_have_prefix = 1;
                pkt->mpeg12_prefix = i;
                pkt->mpeg12_prefix_flags = start_flags | flags;
            }
            continue;
        }

        if (0x00 != code)
            continue;

        if (!pkt->mpeg12_have_picture_timestamp)
        {
            if (i < old_size && i >= old_tail_start)
            {
                j = i - old_tail_start;
                picture_pts = old_tail_pts[j];
                picture_dts = old_tail_dts[j];
            }
            else
            {
                picture_pts = pes->pts;
                picture_dts = pes->dts;
            }
            pkt->mpeg12_picture_pts = picture_pts;
            pkt->mpeg12_picture_dts = picture_dts;
            pkt->mpeg12_picture_flags = start_flags | flags;
            pkt->mpeg12_picture = i;
            pkt->mpeg12_have_picture_timestamp = 1;
        }

        if (i + 5 >= pkt->size)
            break; // wait for picture_coding_type

        picture_type = (pkt->data[i + 5] >> 3) & 0x07;
        boundary = pkt->mpeg12_have_prefix ? pkt->mpeg12_prefix : i;
        if (pkt->mpeg12_have_picture)
        {
            r = handler(param, pes->pn, pes->pid, pkt->codecid, pkt->flags,
                pkt->pts, pkt->dts, pkt->data + keep, boundary - keep);
            if (0 != r)
            {
                mpeg_packet_mpeg12_compact(pkt, pes, old_size, old_tail_start,
                    old_tail_pts, old_tail_dts, old_tail_flags, flags, keep, i);
                mpeg_packet_mpeg12_save_retry(pkt, pes, flush);
                return r;
            }
        }
        else
        {
            keep = boundary;
        }

        pkt->pts = pkt->mpeg12_picture_pts;
        pkt->dts = pkt->mpeg12_picture_dts;
        pkt->sid = pes->sid;
        pkt->codecid = pes->codecid;
        pkt->flags = (pkt->mpeg12_have_prefix ? pkt->mpeg12_prefix_flags : flags)
            | pkt->mpeg12_picture_flags
            | (1 == picture_type ? MPEG_FLAG_IDR_FRAME : 0);
        pkt->mpeg12_have_picture = 1;
        pkt->mpeg12_picture_flags = 0;
        pkt->mpeg12_picture_pts = 0;
        pkt->mpeg12_picture_dts = 0;
        pkt->mpeg12_picture = 0;
        pkt->mpeg12_have_picture_timestamp = 0;
        pkt->mpeg12_have_prefix = 0;
        pkt->mpeg12_prefix = 0;
        pkt->mpeg12_prefix_flags = 0;
        keep = boundary;
    }

    scan = i + 3 < pkt->size ? i : (pkt->size > 3 ? pkt->size - 3 : 0);
    if (flush)
    {
        if (pkt->mpeg12_have_picture)
        {
            boundary = pkt->size;
            if (pkt->mpeg12_have_prefix)
                boundary = pkt->mpeg12_prefix;
            if (pkt->mpeg12_have_picture_timestamp && pkt->mpeg12_picture < boundary)
                boundary = pkt->mpeg12_picture;
            r = handler(param, pes->pn, pes->pid, pkt->codecid, pkt->flags,
                pkt->pts, pkt->dts, pkt->data + keep, boundary - keep);
            if (0 != r)
            {
                mpeg_packet_mpeg12_compact(pkt, pes, old_size, old_tail_start,
                    old_tail_pts, old_tail_dts, old_tail_flags, flags, keep, scan);
                mpeg_packet_mpeg12_save_retry(pkt, pes, flush);
                return r;
            }
        }
        mpeg_packet_mpeg12_reset(pkt);
        return 0;
    }

    mpeg_packet_mpeg12_compact(pkt, pes, old_size, old_tail_start,
        old_tail_pts, old_tail_dts, old_tail_flags, flags, keep, scan);
    return 0;
}

static int mpeg_packet_h264_h265_filter(uint16_t program, uint16_t stream, struct packet_t* pkt, const uint8_t* data, size_t size, pes_packet_handler handler, void* param)
{
    int i;
    size_t off;
    size_t leading;

    //assert(0 == pes->len || pes->payload.len == pes->len);

    // skip AUD
    for (off = i = 0; off < size; off += i + 1)
    {
        i = mpeg_h264_find_nalu(data + off, size - off, &leading);
        if (i < 0)
        {
            assert(0);
            return -1;
        }

        //assert(0 == i - leading);
        if (PSI_STREAM_H264 == pkt->codecid ? 9 == (data[off + i] & 0x1f) : 35 == ((data[off + i] >> 1) & 0x3f))
            continue;

        i -= (int)leading; // rewind to 0x00 00 00 01
        break;
    }

    // TODO: check size > 0 ???
    return handler(param, program, stream, pkt->codecid, pkt->flags, pkt->pts, pkt->dts, data + off + i, size - off - i);
}

// @param[out] consume used of new append data
static int mpeg_packet_h26x(struct packet_t* pkt, const struct pes_t* pes, size_t size, size_t* consume, pes_packet_handler handler, void* param)
{
    int r, n;
    const uint8_t* p, *end, *data;
    h2645_find_new_access find;

    n = PSI_STREAM_H264 == pes->codecid ? 4 : 5;
    data = pkt->data;
    end = pkt->data + pkt->size;
    p = pkt->size - size < n ? pkt->data : end - size - n; // start from trailing nalu
 
    // TODO: The first frame maybe not a valid frame, filter it

    if (0 == pkt->codecid)
    {
        pkt->pts = pes->pts;
        pkt->dts = pes->dts;
        pkt->sid = pes->sid;
        pkt->codecid = pes->codecid;
        pkt->flags = pes->flags;
    }

    // PES contain multiple packet
    find = PSI_STREAM_H264 == pkt->codecid ? mpeg_h264_find_new_access_unit : (PSI_STREAM_H265 == pkt->codecid ? mpeg_h265_find_new_access_unit : mpeg_h266_find_new_access_unit);
    n = find(p, end - p, &pkt->vcl);
    while (n >= 0)
    {
        assert(pkt->vcl > 0);
        if (MPEG_VCL_CORRUPT == pkt->vcl)
        {
            // video data contain 00 00 01 BA
            // maybe previous packet data lost
            r = (p + n - pkt->data) - (pkt->size - *consume);
            assert(r >= 0 && r <= *consume); // r == 0: previous packet lost, new start code find
            *consume = (r < 0 || r > *consume) ? *consume : r;
            pkt->flags |= MPEG_FLAG_PACKET_CORRUPT;
            pkt->size = 0; // clear
            pkt->vcl = 0;
            // todo: handle packet data ???
            return 0;
        }

        p += n;
        pkt->flags = (pkt->flags & (~MPEG_FLAG_IDR_FRAME)) | (1 == pkt->vcl ? MPEG_FLAG_IDR_FRAME : 0); // update key frame flags
        r = mpeg_packet_h264_h265_filter(pes->pn, pes->pid, pkt, data, p - data, handler, param);
        if (0 != r)
            return r;

        data = p;
        pkt->vcl = 0; // next frame
        n = find(p, end - p, &pkt->vcl);
    }

    // save pts/dts
    pkt->pts = pes->pts;
    pkt->dts = pes->dts;
    pkt->sid = pes->sid;
    pkt->flags = pes->flags;
//    assert(0 == find(p, end - p)); // start with AUD

#if !defined(MPEG_KEDA_H265_FROM_H264)
    pkt->codecid = pes->codecid;
#else
    // fix: keda h.265 stream psm codec id incorrect, e.g. psm codec id 36 -> 27 -> 27 -> 27
    if (pkt->codecid != pes->codecid && 0 == mpeg_h26x_verify(data, end - data, &r))
    {
        static const uint8_t sc_codecid[] = { PSI_STREAM_RESERVED, PSI_STREAM_H264, PSI_STREAM_H265, PSI_STREAM_H266, PSI_STREAM_MPEG4, };
        pkt->codecid = sc_codecid[(r < 0 || r >= sizeof(sc_codecid) / sizeof(sc_codecid[0])) ? PSI_STREAM_RESERVED : r];
    }
#endif

    // remain data
    if (data != pkt->data)
    {
        memmove(pkt->data, data, end - data);
        pkt->size = end - data;
    }

    return 0;
}

static void pes_packet_codec_verify(struct pes_t* pes, struct packet_t* pkt)
{
    int r;
    size_t i, n;

#if defined(MPEG_GUESS_STREAM) || defined(MPEG_H26X_VERIFY)
    if (pes->codecid == PSI_STREAM_RESERVED && 0 == mpeg_h26x_verify(pkt->data, pkt->size, &r))
    {
        // modify codecid
        static const uint8_t sc_codecid[] = { PSI_STREAM_RESERVED, PSI_STREAM_H264, PSI_STREAM_H265, PSI_STREAM_H266, PSI_STREAM_MPEG4, };
        pkt->codecid = pes->codecid = sc_codecid[(r < 0 || r >= sizeof(sc_codecid) / sizeof(sc_codecid[0])) ? PSI_STREAM_RESERVED : r];
    }
#endif

#if defined(MPEG_DAHUA_AAC_FROM_G711)
    if ((pes->codecid == PSI_STREAM_AUDIO_G711A || pes->codecid == PSI_STREAM_AUDIO_G711U)
        && pkt->size > 7 && 0xFF == pkt->data[0] && 0xF0 == (pkt->data[1] & 0xF0))
    {
        n = 7;
        // calc mpeg4_aac_adts_frame_length
        for (i = 0; i + 7 < pkt->size && n >= 7; i += n)
        {
            // fix n == 0
            n = ((size_t)(pkt->data[i + 3] & 0x03) << 11) | ((size_t)pkt->data[i + 4] << 3) | ((size_t)(pkt->data[i + 5] >> 5) & 0x07);
        }
        pkt->codecid = pes->codecid = i == pkt->size ? PSI_STREAM_AAC : pes->codecid; // fix it
    }
#endif
}

int pes_packet(struct packet_t* pkt, struct pes_t* pes, const void* data, size_t size, size_t* consume, int start, pes_packet_handler handler, void* param)
{
    int r;
    size_t total;
    struct pes_t retry_pes;

    total = size;
    *consume = size; // all saved
    if (pkt->mpeg12_retry)
    {
        memset(&retry_pes, 0, sizeof(retry_pes));
        retry_pes.pn = pkt->mpeg12_retry_pn;
        retry_pes.pid = pkt->mpeg12_retry_pid;
        retry_pes.sid = pkt->mpeg12_retry_sid;
        retry_pes.codecid = pkt->mpeg12_retry_codecid;
        retry_pes.flags = pkt->mpeg12_retry_flags;
        retry_pes.pts = pkt->mpeg12_retry_pts;
        retry_pes.dts = pkt->mpeg12_retry_dts;
        r = mpeg_packet_mpeg12(pkt, &retry_pes, 0,
            pkt->mpeg12_retry_flush, handler, param);
        if (0 != r)
        {
            *consume = 0;
            return r;
        }
        pkt->mpeg12_retry = 0;
        pkt->mpeg12_retry_flush = 0;
    }

    // TS error flags remain set for the rest of the PES. Use the generation
    // counter so every continuity gap resets the partial MPEG Picture once.
    if (size > 0 && mpeg_packet_is_mpeg12_video(pes->codecid)
        && pkt->mpeg12_discontinuity != pes->discontinuity)
    {
        mpeg_packet_mpeg12_reset(pkt);
        pkt->mpeg12_discontinuity = pes->discontinuity;
    }
    // use timestamp to split packet
    assert(PTS_NO_VALUE != pes->dts);
    if (pkt->size > 0 && (pkt->dts != pes->dts || start)
        // WARNING: don't use pes->codecid
        && PSI_STREAM_H264 != pkt->codecid && PSI_STREAM_H265 != pkt->codecid && PSI_STREAM_H266 != pkt->codecid
        && !mpeg_packet_is_mpeg12_video(pkt->codecid)
        && !mpeg_packet_is_mpeg12_video(pes->codecid))
    {
        if(0 == pes->codecid)
            pes_packet_codec_verify(pes, pkt); // verify on packet complete

        if (PSI_STREAM_H264 != pes->codecid && PSI_STREAM_H265 != pes->codecid && PSI_STREAM_H266 != pes->codecid)
        {
            assert(PTS_NO_VALUE != pkt->dts);
            r = handler(param, pes->pn, pes->pid, pkt->codecid, pkt->flags, pkt->pts, pkt->dts, pkt->data, pkt->size);
            pkt->size = 0; // new packet start
            if (0 != r)
                return r;
        }
        else
        {
            assert(0 == pkt->codecid);
            pkt->codecid = pes->codecid; // update previous packet codec id
            total += pkt->size; // find nalu vcl
        }
    }

    // merge buffer
    r = mpeg_packet_append(pkt, data, size);
    if (0 != r)
        return r;

    if (PSI_STREAM_H264 == pes->codecid || PSI_STREAM_H265 == pes->codecid || PSI_STREAM_H266 == pes->codecid)
    {
        return mpeg_packet_h26x(pkt, pes, total, consume, handler, param);
    }
    else if (mpeg_packet_is_mpeg12_video(pes->codecid))
    {
        return mpeg_packet_mpeg12(pkt, pes, size, NULL == data && 0 == size, handler, param);
    }
    else
    {
        // save pts/dts
        pkt->pts = pes->pts;
        pkt->dts = pes->dts;
        pkt->sid = pes->sid;
        pkt->codecid = pes->codecid;
        pkt->flags = pes->flags;

        // for audio packet only, H.264/H.265 pes->len maybe incorrect
        assert(PSI_STREAM_H264 != pes->codecid && PSI_STREAM_H265 != pes->codecid && PSI_STREAM_H266 != pes->codecid);
#if !defined(MPEG_LIVING_VIDEO_FRAME_DEMUX)
        if (PES_SID_VIDEO != pes->sid)
#endif
        if (pes->len > 0 && pes->pkt.size >= pes->len)
        {
            pes_packet_codec_verify(pes, pkt); // verify on packet complete
            if (PSI_STREAM_H264 == pes->codecid || PSI_STREAM_H265 == pes->codecid || PSI_STREAM_H266 == pes->codecid)
                return mpeg_packet_h26x(pkt, pes, size, consume, handler, param);

            assert(pes->pkt.size == pes->len || (pkt->flags & MPEG_FLAG_PACKET_CORRUPT)); // packet lost
            r = handler(param, pes->pn, pes->pid, pkt->codecid, pkt->flags, pkt->pts, pkt->dts, pes->pkt.data, pes->len);
            pkt->size = 0; // new packet start
        }
    }

    return r;
}
