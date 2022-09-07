#include "mpeg-pes-proto.h"
#include "mpeg-ts-proto.h"
#include "mpeg-util.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#define MPEG_PACKET_PAYLOAD_MAX_SIZE (10 * 1024 * 1024)

typedef int (*h2645_find_new_access)(const uint8_t* p, size_t bytes, int* vcl);

static int mpeg_packet_append(struct packet_t* pkt, const void* data, size_t size)
{
    void* ptr;

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

static int mpeg_packet_h264_h265(struct packet_t* pkt, const struct pes_t* pes, size_t size, pes_packet_handler handler, void* param)
{
    int r, n;
    const uint8_t* p, *end, *data;
    h2645_find_new_access find;

    data = pkt->data;
    end = pkt->data + pkt->size;
    p = pkt->size < size + 5 ? pkt->data : end - size - 5; // start from trailing nalu
    find = PSI_STREAM_H264 == pes->codecid ? mpeg_h264_find_new_access_unit : mpeg_h265_find_new_access_unit;

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
    n = find(p, end - p, &pkt->vcl);
    while (n >= 0)
    {
        assert(pkt->vcl > 0);

        p += n;
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
    pkt->codecid = pes->codecid;
    pkt->flags = pes->flags;
//    assert(0 == find(p, end - p)); // start with AUD

    // remain data
    if (data != pkt->data)
    {
        memmove(pkt->data, data, end - data);
        pkt->size = end - data;
    }

    return 0;
}

int pes_packet(struct packet_t* pkt, const struct pes_t* pes, const void* data, size_t size, int start, pes_packet_handler handler, void* param)
{
    int r;

    if (PSI_STREAM_H264 == pes->codecid || PSI_STREAM_H265 == pes->codecid)
    {
        r = mpeg_packet_append(pkt, data, size);
        if (0 != r)
            return r;

        return mpeg_packet_h264_h265(pkt, pes, size, handler, param);
    }
    else
    {
        // use timestamp to split packet
        assert(PTS_NO_VALUE != pes->dts);
        if (pkt->size > 0 && (pkt->dts != pes->dts || start))
        {
            assert(PTS_NO_VALUE != pkt->dts);
            r = handler(param, pes->pn, pes->pid, pkt->codecid, pkt->flags, pkt->pts, pkt->dts, pkt->data, pkt->size);
            pkt->size = 0; // new packet start
            if (0 != r)
                return r;
        }

        r = mpeg_packet_append(pkt, data, size);
        if (0 != r)
            return r;

        // save pts/dts
        pkt->pts = pes->pts;
        pkt->dts = pes->dts;
        pkt->sid = pes->sid;
        pkt->codecid = pes->codecid;
        pkt->flags = pes->flags;

        // for audio packet only, H.264/H.265 pes->len maybe incorrect
        assert(PSI_STREAM_H264 != pes->codecid && PSI_STREAM_H265 != pes->codecid);
#if !defined(MPEG_LIVING_VIDEO_FRAME_DEMUX)
        if (PES_SID_VIDEO != pes->sid)
#endif
        if (pes->len > 0 && pes->pkt.size >= pes->len)
        {
            assert(pes->pkt.size == pes->len || (pkt->flags & MPEG_FLAG_PACKET_CORRUPT)); // packet lost
            r = handler(param, pes->pn, pes->pid, pkt->codecid, pkt->flags, pkt->pts, pkt->dts, pes->pkt.data, pes->len);
            pkt->size = 0; // new packet start
        }
    }

    return r;
}
