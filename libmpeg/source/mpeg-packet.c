#include "mpeg-pes-proto.h"
#include "mpeg-ts-proto.h"
#include "mpeg-util.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

static int mpeg_packet_append(struct packet_t* pkt, const void* data, size_t size)
{
    void* ptr;

    if (pkt->capacity < pkt->size + size)
    {
        ptr = realloc(pkt->data, pkt->size + size + 2048);
        if (NULL == ptr) return ENOMEM;
        pkt->data = (uint8_t*)ptr;
        pkt->capacity = pkt->size + size + 2048;
    }

    // append new data
    memcpy(pkt->data + pkt->size, data, size);
    pkt->size += size;
    return 0;
}

typedef int (*h2645_find_aud)(const uint8_t* p, size_t bytes);

static int mpeg_packet_h264_h265_hasaud(struct packet_t* pkt, int codecid, const uint8_t* data, size_t size)
{
	h2645_find_aud find;
	find = PSI_STREAM_H264 == codecid ? find_h264_access_unit_delimiter : find_h265_access_unit_delimiter;

	if (pkt->size >= 4)
		return 0 == find(pkt->data, pkt->size > 10 ? 10 : pkt->size) ? 1 : 0;
	return 0 == find(data, size > 10 ? 10 : size) ? 1 : 0;
}

static int mpeg_packet_h264_h265_filter(uint16_t program, struct packet_t* pkt, const uint8_t* data, size_t size, h2645_find_aud find, pes_packet_handler handler, void* param)
{
    int i;

    //assert(0 == pes->len || pes->payload.len == pes->len);

    // filter AUD
    assert(0 == find(data, size));
    assert(-1 == find(data + 5, size - 5));
    i = h264_find_nalu(data + 5, size - 5);
    if(-1 != i)
        handler(param, program, pkt->sid, pkt->codecid, pkt->flags, pkt->pts, pkt->dts, data + i + 5, size - i - 5);

    return 0;
}

static int mpeg_packet_h264_h265(struct packet_t* pkt, const struct pes_t* pes, size_t size, pes_packet_handler handler, void* param)
{
    int r, aud;
    h2645_find_aud find;
    const uint8_t* p, *p0, *end;

    p = pkt->data;
    end = pkt->data + pkt->size;
    find = PSI_STREAM_H264 == pes->codecid ? find_h264_access_unit_delimiter : find_h265_access_unit_delimiter;

    // previous packet
    if (pkt->size > size)
    {
        p = end - size - 3; // handle trailing AUD, <0, 0, 1, AUD>

        aud = p < pkt->data + 3 ? -1 : find(p, end - p);
        if (-1 == aud)
            return 0; // need more data

        p += aud;
        r = mpeg_packet_h264_h265_filter(pes->pn, pkt, pkt->data, p - pkt->data, find, handler, param);
    }

    // save pts/dts
    pkt->pts = pes->pts;
    pkt->dts = pes->dts;
    pkt->sid = pes->sid;
    pkt->codecid = pes->codecid;
    pkt->flags = pes->data_alignment_indicator ? 1 : 0;
    assert(0 == find(p, end - p)); // start with AUD

    // PES contain multiple packet
    aud = find(p + 5, end - p - 5);
    for (p0 = p; -1 != aud; p0 = p)
    {
        p += aud + 5; // next packet
        r = mpeg_packet_h264_h265_filter(pes->pn, pkt, p0, p - p0, find, handler, param);

        aud = find(p + 5, end - p - 5);
    }

    // remain data
    if (p != pkt->data)
    {
        memmove(pkt->data, p, end - p);
        pkt->size = end - p;
    }

    return 0;
}

int pes_packet(struct packet_t* pkt, const struct pes_t* pes, const void* data, size_t size, pes_packet_handler handler, void* param)
{
    int r;

	// split H.264/H.265 by AUD
    if ( (PSI_STREAM_H264 == pes->codecid || PSI_STREAM_H265 == pes->codecid) && mpeg_packet_h264_h265_hasaud(pkt, pes->codecid, data, size) )
    {
        r = mpeg_packet_append(pkt, data, size);
        if (0 != r)
            return r;

        return mpeg_packet_h264_h265(pkt, pes, size, handler, param);
    }
    else if (0 == pes->len || PSI_STREAM_H264 == pes->codecid || PSI_STREAM_H265 == pes->codecid)
    {
        // use timestamp to split packet
        assert(PTS_NO_VALUE != pes->dts);
        if (pkt->size > 0 && pkt->dts != pes->dts)
        {
            assert(PTS_NO_VALUE != pkt->dts);
            handler(param, pes->pn, pkt->sid, pkt->codecid, pkt->flags, pkt->pts, pkt->dts, pkt->data, pkt->size);
            pkt->size = 0; // new packet start
        }

        r = mpeg_packet_append(pkt, data, size);
        if (0 != r)
            return r;

        // save pts/dts
        pkt->pts = pes->pts;
        pkt->dts = pes->dts;
        pkt->sid = pes->sid;
        pkt->codecid = pes->codecid;
        pkt->flags = pes->data_alignment_indicator ? 1 : 0;
    }
    else
    {
        r = mpeg_packet_append(pkt, data, size);
        if (0 != r)
            return r;

        assert(pes->len > 0);
        if (pes->pkt.size >= pes->len && pes->len > 0)
        {
            assert(pes->pkt.size == pes->len);
            pkt->flags = pes->data_alignment_indicator ? 1 : 0; // flags
            handler(param, pes->pn, pes->sid, pes->codecid, pkt->flags, pes->pts, pes->dts, pes->pkt.data, pes->len);
            pkt->size = 0; // new packet start
        }
    }

    return 0;
}
