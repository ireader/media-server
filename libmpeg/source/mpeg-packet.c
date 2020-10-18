#include "mpeg-pes-proto.h"
#include "mpeg-ts-proto.h"
#include "mpeg-util.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#define MPEG_PACKET_PAYLOAD_MAX_SIZE (10 * 1024 * 1024)

static int mpeg_packet_append(struct packet_t* pkt, const void* data, size_t size)
{
    void* ptr;

    if (pkt->capacity < pkt->size + size)
    {
        if (pkt->size + size > MPEG_PACKET_PAYLOAD_MAX_SIZE)
            return EINVAL;
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

typedef int (*h2645_find_aud)(const uint8_t* p, size_t bytes, size_t* leading);

static int mpeg_packet_h264_h265_start_width_aud(int codecid, const uint8_t* data, size_t size)
{
    int r;
    size_t leading;
    r = mpeg_h264_find_nalu(data, size, &leading);
	return -1 != r && 0 == r - (int)leading && (PSI_STREAM_H264 == codecid ? 9 == (data[r] & 0x1f) : 35 == ((data[r] >> 1) & 0x3f))? 1 : 0;
}

static int mpeg_packet_h264_h265_is_new_access(int codecid, const uint8_t* data, size_t size)
{
    size_t i;
    for(i = 0; i + 1 < size; i++)
    {
        if(0x00 == data[i])
            continue;
        
        if (0x01 != data[i] || i < 2)
            return 0;
        
        switch(codecid)
        {
        case PSI_STREAM_H264: return mpeg_h264_is_new_access_unit(data + i + 1, size - i - 1);
        case PSI_STREAM_H265: return mpeg_h265_is_new_access_unit(data + i + 1, size - i - 1);
        default: return 0;
        }
    }
    
    return 0;
}

static int mpeg_packet_h264_h265_filter(uint16_t program, struct packet_t* pkt, const uint8_t* data, size_t size, pes_packet_handler handler, void* param)
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

        i -= leading; // rewind to 0x00 00 00 01
        break;
    }

    // TODO: check size > 0 ???
    return handler(param, program, pkt->sid, pkt->codecid, pkt->flags, pkt->pts, pkt->dts, data + off + i, size - off - i);
}

static int mpeg_packet_h264_h265(struct packet_t* pkt, const struct pes_t* pes, size_t size, pes_packet_handler handler, void* param)
{
    size_t leading;
    int r, aud, off;
    h2645_find_aud find;
    const uint8_t* p, *end;

    p = pkt->data;
    end = pkt->data + pkt->size;
    find = PSI_STREAM_H264 == pes->codecid ? mpeg_h264_find_access_unit_delimiter : mpeg_h265_find_access_unit_delimiter;
    leading = 0;

    // previous packet
    if (pkt->size > size)
    {
        p = end - size - 3; // handle trailing AUD, <0, 0, 1, AUD>

        aud = p < pkt->data + 4 ? -1 : find(p, end - p, &leading);
        if (aud < 0)
            return 0; // need more data

        p += aud - leading; // next frame
        r = mpeg_packet_h264_h265_filter(pes->pn, pkt, pkt->data, p - pkt->data, handler, param);
        if (0 != r)
            return r;

        aud = leading;
    }
    else
    {
        // location first AUD
        aud = find(pkt->data, pkt->size, &leading);
        if (aud < 0)
            return pkt->size > MPEG_PACKET_PAYLOAD_MAX_SIZE ? -1 : 0; // need more data
    }

    // save pts/dts
    pkt->pts = pes->pts;
    pkt->dts = pes->dts;
    pkt->sid = pes->sid;
    pkt->codecid = pes->codecid;
    pkt->flags = pes->data_alignment_indicator ? 1 : 0;
//    assert(0 == find(p, end - p)); // start with AUD

    // PES contain multiple packet
    assert(p + aud <= end);
    while (p + aud < end)
    {
        off = aud; // frame start AUD
        aud = find(p + off, end - p - off, &leading);
        if (aud < 0)
            break;

        assert(aud >= (int)leading);
        r = mpeg_packet_h264_h265_filter(pes->pn, pkt, p, off + aud - leading, handler, param);
        if (0 != r)
            return r;

        p += off + aud - leading; // next frame
    }

    // remain data
    if (p != pkt->data)
    {
        memmove(pkt->data, p, end - p);
        pkt->size = end - p;
    }

    return 0;
}

int pes_packet(struct packet_t* pkt, const struct pes_t* pes, const void* data, size_t size, int start, pes_packet_handler handler, void* param)
{
    int r;
    static uint8_t h264aud[] = { 0, 0, 0, 1, 0x09, 0xE0 };
    static uint8_t h265aud[] = { 0, 0, 0, 1, 0x46, 0x01, 0x50 };
    
	// split H.264/H.265 by AUD
    if ( (PSI_STREAM_H264 == pes->codecid || PSI_STREAM_H265 == pes->codecid) && mpeg_packet_h264_h265_start_width_aud(pes->codecid, pkt->data, pkt->size))
    {
#if 1 // for some stream only has an AUD in IDR frame
        if(mpeg_packet_h264_h265_is_new_access(pes->codecid, data, size) && 0 == mpeg_packet_h264_h265_start_width_aud(pes->codecid, data, size))
        {
            assert(PTS_NO_VALUE != pkt->dts);
            r = PSI_STREAM_H264 == pes->codecid ? mpeg_packet_append(pkt, h264aud, sizeof(h264aud)) : mpeg_packet_append(pkt, h265aud, sizeof(h265aud));
            if (0 != r)
                return r;
        }
#endif
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
            r = handler(param, pes->pn, pkt->sid, pkt->codecid, pkt->flags, pkt->pts, pkt->dts, pkt->data, pkt->size);
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
        pkt->flags = pes->data_alignment_indicator ? 1 : 0;

        // for audio packet only, H.264/H.265 pes->len maybe incorrect
        if (pes->len > 0 && pes->pkt.size >= pes->len && PSI_STREAM_H264 != pes->codecid && PSI_STREAM_H265 != pes->codecid)
        {
            assert(pes->pkt.size == pes->len); // packet lost
            r = handler(param, pes->pn, pkt->sid, pkt->codecid, pkt->flags, pkt->pts, pkt->dts, pes->pkt.data, pes->len);
            pkt->size = 0; // new packet start
        }
    }

    return r;
}
