// ITU-T H.222.0(06/2012)
// Information technology - Generic coding of moving pictures and associated audio information: Systems
// 2.5.3.1 Program stream(p74)

#include <stdio.h>
#include <stdlib.h>
#include "mpeg-ps.h"
#include "mpeg-ps-internal.h"
#include "mpeg-pes-internal.h"
#include "mpeg-util.h"
#include <assert.h>
#include <string.h>
#include <errno.h>

#define N_BUFFER_INIT   256
#define N_BUFFER_MAX    (512*1024)
#define N_BUFFER_INC    (8000)

enum ps_demuxer_state_t
{
    PS_DEMUXER_STATE_START = 0,
    PS_DEMUXER_STATE_DATA,
};

struct ps_demuxer_t
{
    struct psm_t psm;
    struct psd_t psd;

    struct ps_pack_header_t pkhd;
    struct ps_system_header_t system;

    enum ps_demuxer_state_t state;
    struct pes_t* pes;
    size_t pes_length;
    struct
    {
        uint8_t* ptr;
        size_t len, cap;
    } buffer;

    int start;
    int sync;

    ps_demuxer_onpacket onpacket;
	void* param;

    struct ps_demuxer_notify_t notify;
    void* notify_param;
};

static void ps_demuxer_notify(struct ps_demuxer_t* ps);

static int ps_demuxer_find_startcode(struct mpeg_bits_t *reader)
{
    uint8_t v8;
    size_t zeros;

    zeros = 0;
    while (1)
    {
        v8 = mpeg_bits_read8(reader);
        if (0 != mpeg_bits_error(reader))
            break;

        if (0x01 == v8 && zeros >= 2)
            return 0;

        zeros = 0x00 != v8 ? 0 : (zeros + 1);
    }

    return -1;
}

#if defined(MPEG_ZERO_PAYLOAD_LENGTH)
static int ps_demuxer_find_pes_start(struct mpeg_bits_t* reader)
{
    size_t origin, offset;
    origin = mpeg_bits_tell(reader);
    while (0 == mpeg_bits_error(reader))
    {
        if (0 != ps_demuxer_find_startcode(reader))
            break;

        offset = mpeg_bits_tell(reader);
        if (PES_SID_START == mpeg_bits_read8(reader) && 0 == mpeg_bits_error(reader))
        {
            mpeg_bits_seek(reader, origin);
            return (int)(offset - origin - 3);
        }
        mpeg_bits_seek(reader, offset); // restore
    }

    mpeg_bits_seek(reader, origin);
    return -1;
}
#endif

static int ps_sync_start_probe(struct mpeg_bits_t* reader)
{
    if (0x000001 == mpeg_bits_tryread(reader, 3))
        return MPEG_ERROR_OK;

    if (mpeg_bits_tell(reader) + 3 > mpeg_bits_length(reader))
        return MPEG_ERROR_NEED_MORE_DATA;

    return MPEG_ERROR_INVALID_DATA;
}

static int ps_demuxer_onpes(void* param, int program, int stream, int codecid, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes)
{
    struct ps_demuxer_t* ps;
    ps = (struct ps_demuxer_t*)param;
    assert(0 == program); // unused(ts demux only)
    return ps->onpacket(ps->param, stream, codecid, flags, pts, dts, data, bytes);
}

static struct pes_t* psm_fetch(struct psm_t* psm, uint8_t sid)
{
    size_t i;
    for (i = 0; i < psm->stream_count; ++i)
    {
        if (psm->streams[i].sid == sid)
            return &psm->streams[i];
    }

    if (psm->stream_count < sizeof(psm->streams) / sizeof(psm->streams[0]))
    {
		// '110x xxxx'
		// ISO/IEC 13818-3 or ISO/IEC 11172-3 or ISO/IEC 13818-7 or 
		// ISO/IEC 14496-3 or ISO/IEC 23008-3 audio stream number 'x xxxx'

		// '1110 xxxx'
		// Rec. ITU-T H.262 | ISO/IEC 13818-2, ISO/IEC 11172-2, ISO/IEC 14496-2, 
		// Rec. ITU-T H.264 | ISO/IEC 14496-10 or 
		// Rec. ITU-T H.265 | ISO/IEC 23008-2 video stream number 'xxxx'

        // guess stream codec id
#if defined(MPEG_GUESS_STREAM) || defined(MPEG_H26X_VERIFY)
        if (0xE0 <= sid && sid <= 0xEF)
        {
            psm->streams[psm->stream_count].pid = sid;
            psm->streams[psm->stream_count].codecid = PSI_STREAM_RESERVED; // unknown
            return &psm->streams[psm->stream_count++];
        }
#endif
#if defined(MPEG_GUESS_STREAM)
        if(0xC0 <= sid && sid <= 0xDF)
        {
            psm->streams[psm->stream_count].pid = sid;
            psm->streams[psm->stream_count].codecid = PSI_STREAM_AAC;
            return &psm->streams[psm->stream_count++];
        }
#endif
    }

    return NULL;
}

static int ps_demuxer_packet(struct ps_demuxer_t *ps, const uint8_t* data, size_t bytes, size_t *consume)
{
    int r;
    struct pes_t* pes;

    pes = ps->pes;

#if defined(MPEG_LIVING_VIDEO_FRAME_DEMUX)
    // video packet size > 0xFFFF, split by pts/dts
    if (PES_SID_VIDEO == pes->sid
        && PSI_STREAM_H264 != pes->codecid && PSI_STREAM_H265 != pes->codecid && PSI_STREAM_H266 != pes->codecid
        && (pes->pkt.size > 0 || pes->len + pes->PES_header_data_length + 3 == 0xFFFF))
        pes->len = 0;
#endif

    pes->flags = pes->data_alignment_indicator ? MPEG_FLAG_IDR_FRAME : 0;
    r = pes_packet(&pes->pkt, pes, data, bytes, consume, ps->start, ps_demuxer_onpes, ps);
    ps->start = 0; // clear start flags
    return r;
}

static int ps_demuxer_skip(struct ps_demuxer_t* ps, struct mpeg_bits_t* reader, uint8_t v8)
{
    // '110x xxxx' ISO/IEC 13818-3 or ISO/IEC 11172-3 or ISO/IEC 13818-7 or ISO/IEC 14496-3 or ISO/IEC 23008-3 audio stream number 'x xxxx'
    // '1110 xxxx' Rec. ITU-T H.262 | ISO/IEC 13818-2, ISO/IEC 11172-2, ISO/IEC 14496-2, Rec. ITU-T H.264 | ISO/IEC 14496-10, Rec. ITU-T H.265 | ISO/IEC 23008-2, Rec. ITU-T H.266 | ISO/IEC 23090-3 or ISO/IEC 23094-1 video stream number 'xxxx'
    // '1111 1101' extended_stream_id
    if ((v8 >= 0xC0 && v8 <= 0xDF) || (v8 >= 0xE0 && v8 <= 0xEF) || v8 == PES_SID_PRIVATE_1 || v8 == 0xFD)
    {
        // skip PES
        mpeg_bits_skip(reader, mpeg_bits_read16(reader));
        if (mpeg_bits_error(reader))
            return MPEG_ERROR_NEED_MORE_DATA;
    }
    else
    {
        // backward 1-step
        mpeg_bits_seek(reader, mpeg_bits_tell(reader) - 1);
        assert(0 == mpeg_bits_error(reader));
        ps->sync = 0; // wait for next start packet
    }

    return MPEG_ERROR_OK;
}

static int ps_demuxer_header(struct ps_demuxer_t* ps, struct mpeg_bits_t* reader)
{
    int r;
    size_t n;
	size_t off;
    uint8_t v8;
    struct pes_t* pes;

    for (off = mpeg_bits_tell(reader); 0 == ps_demuxer_find_startcode(reader); off = mpeg_bits_tell(reader))
    {
        r = MPEG_ERROR_OK;
        v8 = mpeg_bits_read8(reader);
        if (mpeg_bits_error(reader))
            break;

        if (!ps->sync && v8 != PES_SID_START)
        {
            // backward 1-step
            mpeg_bits_seek(reader, mpeg_bits_tell(reader) - 1);
            assert(0 == mpeg_bits_error(reader));
            continue; // wait for 00 00 01 BA
        }

        // fix HIK H.265: 00 00 01 BA 00 00 01 E0 ...
        if (0x000001 == (mpeg_bits_tryread(reader, 3) & 0xFFFFFF))
            continue;

        switch (v8)
        {
        case PES_SID_START:
            r = pack_header_read(&ps->pkhd, reader);
            r = MPEG_ERROR_OK == r ? ps_sync_start_probe(reader) : r;
            ps->start = MPEG_ERROR_OK == r ? 1 : ps->start;
            ps->sync = MPEG_ERROR_OK == r ? 1 : 0;
            break;
            
        case PES_SID_SYS:
            r = system_header_read(&ps->system, reader);
            break;
            
        case PES_SID_END:
            r = MPEG_ERROR_OK;
            break;

        case PES_SID_PSM:
            n = ps->psm.stream_count;
            r = psm_read(&ps->psm, reader);
            if (n != ps->psm.stream_count)
                ps_demuxer_notify(ps); // TODO: check psm stream sid
            break;

        case PES_SID_PSD:
            r = psd_read(&ps->psd, reader);
            break;

        case PES_SID_PRIVATE_2:
        case PES_SID_ECM:
        case PES_SID_EMM:
        case PES_SID_DSMCC:
        case PES_SID_H222_E:
            // stream data
        case PES_SID_PADDING:
            // padding
            mpeg_bits_skip(reader, mpeg_bits_read16(reader));
            r = mpeg_bits_error(reader) ? MPEG_ERROR_NEED_MORE_DATA : MPEG_ERROR_OK;
            break;

        // ffmpeg mpeg.c mpegps_read_pes_header
        //case 0x1c0:
        //case 0x1df:
        //case 0x1e0:
        //case 0x1ef:
        //case 0x1bd:
        //case 0x01fd:
        //	break;

        default:
            pes = psm_fetch(&ps->psm, v8);
            if (pes)
            {
                pes->sid = v8;
                r = ps->pkhd.mpeg2 ? pes_read_header(pes, reader) : pes_read_mpeg1_header(pes, reader);
                if (MPEG_ERROR_OK == r)
                {
#if defined(MPEG_ZERO_PAYLOAD_LENGTH)
                    // fix H.264/H.265 ps payload zero-length
                    if (0 == pes->len && 0xE0 <= pes->sid && pes->sid <= 0xEF)
                    {
                        r = ps_demuxer_find_pes_start(reader);
                        if (-1 == r)
                            return (int)off;

                        pes->len = r;
                        r = MPEG_ERROR_OK;
                    }
#endif

                    ps->pes = pes;
                    ps->pes_length = 0; // init
                    ps->state = PS_DEMUXER_STATE_DATA; // next state: handle packet
                    return (int)mpeg_bits_tell(reader);
                }
            }
            else // pes == NULL
            {
                r = ps_demuxer_skip(ps, reader, v8);
            }
            break;
        }

        if (r < 0)
            return r; // user handler error
        else if (r == MPEG_ERROR_NEED_MORE_DATA)
            break;
    }

    return (int)off;
}

static int ps_demuxer_buffer(struct ps_demuxer_t* ps, const uint8_t* data, size_t bytes)
{
    void* ptr;
    if (ps->buffer.len + bytes > ps->buffer.cap)
    {
        if (ps->buffer.len + bytes > N_BUFFER_MAX)
            return -E2BIG;

        ptr = realloc(ps->buffer.ptr == (uint8_t*)(ps + 1) ? NULL : ps->buffer.ptr, ps->buffer.len + bytes + N_BUFFER_INC);
        if (!ptr)
            return -ENOMEM;

        if (ps->buffer.ptr == (uint8_t*)(ps + 1))
            memcpy(ptr, ps->buffer.ptr, ps->buffer.len);
        ps->buffer.ptr = (uint8_t*)ptr;
        ps->buffer.cap = ps->buffer.len + bytes + N_BUFFER_INC;
    }

    memmove(ps->buffer.ptr + ps->buffer.len, data, bytes);
    ps->buffer.len += bytes;
    return 0;
}

int ps_demuxer_input(struct ps_demuxer_t* ps, const uint8_t* data, size_t bytes)
{
    int r;
    size_t i, consume;
    struct mpeg_bits_t reader;

    for(i = 0; i < bytes; )
    {
        switch (ps->state)
        {
        case PS_DEMUXER_STATE_START:
            mpeg_bits_init2(&reader, ps->buffer.ptr, ps->buffer.len, data + i, bytes - i);

            r = ps_demuxer_header(ps, &reader);
            if (r >= 0)
            {
                assert(r <= ps->buffer.len + (bytes - i));
                if (r >= ps->buffer.len)
                {
                    r -= (int)ps->buffer.len;
                    i += r;
                    ps->buffer.len = 0;
                }
                else if(r > 0)
                {
                    memmove(ps->buffer.ptr, ps->buffer.ptr + r, ps->buffer.len - r);
                    ps->buffer.len -= r;
                }
                else
                {
                    assert(0 == r);
                }

                if (PS_DEMUXER_STATE_START == ps->state && i < bytes)
                {
                    // stash buffer
                    r = ps_demuxer_buffer(ps, data + i, bytes - i);
                    return 0 == r ? (int)bytes : r;
                }
            }
            break;

        case PS_DEMUXER_STATE_DATA:
            assert(0 == ps->buffer.len);

            assert(ps->pes && ps->pes_length <= ps->pes->len);
            if (bytes - i >= ps->pes->len - ps->pes_length)
            {
                r = ps_demuxer_packet(ps, data + i, ps->pes->len - ps->pes_length, &consume);
                i += consume; // ps->pes->len - ps->pes_length; // fix: pack start code in video data
                ps->pes_length = 0;
                ps->start = 0; // clear start flag
                ps->state = PS_DEMUXER_STATE_START; // next round
            }
            else
            {
                // need more data
                r = ps_demuxer_packet(ps, data + i, bytes - i, &consume);
                ps->pes_length = consume == bytes - i ? ps->pes_length + (bytes - i) : 0;
                ps->start = consume == bytes - i ? ps->start : 0;
                ps->state = consume == bytes - i ? ps->state : PS_DEMUXER_STATE_START;
                i += consume; //i = bytes; // fix: pack start code in video data                
            }

            break;

        default:
            assert(0);
            return -1;
        }

        if (r < 0)
        {
            ps->state = PS_DEMUXER_STATE_START; // skip error, try find next start code
            ps->buffer.len = 0; // clear buffer
            return r;
        }
    }

    return (int)bytes;
}

struct ps_demuxer_t* ps_demuxer_create(ps_demuxer_onpacket onpacket, void* param)
{
	struct ps_demuxer_t* ps;
	ps = calloc(1, sizeof(struct ps_demuxer_t) + N_BUFFER_INIT);
	if(!ps)
		return NULL;

    ps->state = PS_DEMUXER_STATE_START;
    ps->pkhd.mpeg2 = 1;
    ps->onpacket = onpacket;
	ps->param = param;

    ps->buffer.ptr = (uint8_t*)(ps + 1);
    ps->buffer.cap = N_BUFFER_INIT;
	return ps;
}

int ps_demuxer_destroy(struct ps_demuxer_t* ps)
{
    size_t i;
    struct pes_t* pes;
    for (i = 0; i < ps->psm.stream_count; i++)
    {
        pes = &ps->psm.streams[i];
        if (pes->pkt.data)
            free(pes->pkt.data);
        pes->pkt.data = NULL;
    }

    if (ps->buffer.ptr != (uint8_t*)(ps + 1))
    {
        assert(ps->buffer.cap > N_BUFFER_INIT);
        free(ps->buffer.ptr);
        ps->buffer.ptr = NULL;
    }
	free(ps);
	return 0;
}

void ps_demuxer_set_notify(struct ps_demuxer_t* ps, struct ps_demuxer_notify_t *notify, void* param)
{
    ps->notify_param = param;
    memcpy(&ps->notify, notify, sizeof(ps->notify));
}

static void ps_demuxer_notify(struct ps_demuxer_t* ps)
{
    size_t i;
    struct pes_t* pes;
    if (!ps->notify.onstream)
        return;

    for (i = 0; i < ps->psm.stream_count; i++)
    {
        pes = &ps->psm.streams[i];
        ps->notify.onstream(ps->notify_param, pes->pid, pes->codecid, pes->esinfo, pes->esinfo_len, i + 1 >= ps->psm.stream_count ? 1 : 0);
    }
}
