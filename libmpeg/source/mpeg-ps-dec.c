// ITU-T H.222.0(06/2012)
// Information technology - Generic coding of moving pictures and associated audio information: Systems
// 2.5.3.1 Program stream(p74)

#include <stdio.h>
#include <stdlib.h>
#include "mpeg-ps.h"
#include "mpeg-ps-proto.h"
#include "mpeg-ts-proto.h"
#include "mpeg-pes-proto.h"
#include <assert.h>
#include <string.h>

#define MPEG_GUESS_STREAM
struct ps_demuxer_t
{
    struct psm_t psm;
    struct psd_t psd;

    struct ps_pack_header_t pkhd;
    struct ps_system_header_t system;

    int start;

    ps_demuxer_onpacket onpacket;
	void* param;

    struct ps_demuxer_notify_t notify;
    void* notify_param;
};

static void ps_demuxer_notify(struct ps_demuxer_t* ps);

static size_t ps_demuxer_find_startcode(const uint8_t* data, size_t bytes)
{
    const uint8_t* p, * pend;
    pend = data + bytes;
    p = data;

    // find ps start
    for (p = data; data && p + 2 < pend; p++)
    {
        if (0x00 != p[0])
            continue;

        if (0x00 != p[1])
        {
            p++;
            continue;
        }

        if (0x01 == p[2])
            break;
        else if (0x00 != p[2])
            p += 2;
    }

    return p - data;
}

#if defined(MPEG_ZERO_PAYLOAD_LENGTH)
static size_t ps_demuxer_find_pes_start(const uint8_t* data, size_t bytes)
{
    size_t i;
    const uint8_t* p, * end;
    
    end = data + bytes;
    for (p = data; p + 6 < end; p += i + 4)
    {
        i = ps_demuxer_find_startcode(p, end - p);
        if (PES_SID_START == p[i + 3])
            return i + (p - data);
    }

    return bytes; // not found
}
#endif

static int ps_demuxer_onpes(void* param, int program, int stream, int codecid, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes)
{
    struct ps_demuxer_t* ps;
    ps = (struct ps_demuxer_t*)param;
    assert(0 == program); // unused(ts demux only)
    return ps->onpacket(ps->param, stream, codecid, flags, pts, dts, data, bytes);
}

int mpeg_find_h265_nalu_type(const uint8_t* p, size_t bytes,uint8_t nal_type)
{

    size_t i, zeros;
    for (zeros = i = 0; i + 1 < bytes; i++)
    {
        if (0x01 == p[i] && zeros >= 2)
        {
            assert(i >= zeros);

            uint8_t sid = p[i+1];

            if ((0xE0 <= sid && sid <= 0xEF) || (0xC0 <= sid && sid <= 0xDF) || (0xBD == sid) || (0xFD == sid))
            {
                zeros = 0;
                continue;
            }
            uint8_t current_nal_type = (p[i+1] >> 1) & 0x3f;
            if(nal_type == current_nal_type)
            {
                return 1;
            }

            zeros = 0;
            continue;
        }

        zeros = 0x00 != p[i] ? 0 : (zeros + 1);
    }

    return 0;
}

int mpeg_find_h264_nalu_type(const uint8_t* p, size_t bytes,uint8_t nal_type)
{

    size_t i, zeros;
    for (zeros = i = 0; i + 1 < bytes; i++)
    {
        if (0x01 == p[i] && zeros >= 2)
        {
            assert(i >= zeros);

            uint8_t sid = p[i+1];

            if ((0xE0 <= sid && sid <= 0xEF) || (0xC0 <= sid && sid <= 0xDF) || (0xBD == sid) || (0xFD == sid))
            {
                zeros = 0;
                continue;
            }
            uint8_t current_nal_type = p[i+1] & 0x1f;
            if(nal_type == current_nal_type)
            {
                return 1;
            }

            zeros = 0;
            continue;
        }

        zeros = 0x00 != p[i] ? 0 : (zeros + 1);
    }

    return 0;
}

static struct pes_t* psm_fetch(struct psm_t* psm, uint8_t sid,const uint8_t* data, size_t bytes)
{
    size_t i;
    for (i = 0; i < psm->stream_count; ++i)
    {
        if (psm->streams[i].sid == sid)
            return &psm->streams[i];
    }

#if defined(MPEG_GUESS_STREAM)
    if(psm->find_psm == 0)
    {
        if (psm->stream_count < sizeof(psm->streams) / sizeof(psm->streams[0]))
        {
            // '110x xxxx'
            // ISO/IEC 13818-3 or ISO/IEC 11172-3 or ISO/IEC 13818-7 or
            // ISO/IEC 14496-3 or ISO/IEC 23008-3 audio stream number 'x xxxx'

            // '1110 xxxx'
            // Rec. ITU-T H.262 | ISO/IEC 13818-2, ISO/IEC 11172-2, ISO/IEC 14496-2,
            // Rec. ITU-T H.264 | ISO/IEC 14496-10 or
            // Rec. ITU-T H.265 | ISO/IEC 23008-2 video stream number 'xxxx'
            int findGuesstVideoSream = 0;
            int findGuesstAudioSream = 0;
            // guess stream codec id
            if (0xE0 <= sid && sid <= 0xEF)
            {
                enum { H264_NAL_IDR = 5, H264_NAL_SPS = 7, H264_NAL_PPS = 8 };
                int findH264SPS = mpeg_find_h264_nalu_type(data,bytes,H264_NAL_SPS);
                int findH264PPS = mpeg_find_h264_nalu_type(data,bytes,H264_NAL_PPS);
                int findH264IDR = mpeg_find_h264_nalu_type(data,bytes,H264_NAL_IDR);
                uint8_t codecid = PSI_STREAM_H264;
                if(findH264SPS == 1 && findH264PPS == 1 && findH264IDR == 1)
                {
                   codecid = PSI_STREAM_H264;
                   findGuesstVideoSream = 1;
                }
                else
                {
                    enum { H265_NAL_VPS = 32, H265_NAL_SPS = 33, H265_NAL_PPS = 34,H265_NAL_IDR = 19};
                    int findH265VPS = mpeg_find_h265_nalu_type(data,bytes,H265_NAL_VPS);
                    int findH265SPS = mpeg_find_h265_nalu_type(data,bytes,H265_NAL_SPS);
                    int findH265PPS = mpeg_find_h265_nalu_type(data,bytes,H265_NAL_PPS);
                    int findH265IDR = mpeg_find_h265_nalu_type(data,bytes,H265_NAL_IDR);

                    if(findH265VPS == 1 && findH265SPS == 1 && findH265PPS == 1 && findH265IDR == 1)
                    {
                       codecid = PSI_STREAM_H265;
                       findGuesstVideoSream = 1;
                    }
                }
                if(findGuesstVideoSream == 1)
                {
                    psm->streams[psm->stream_count].codecid = codecid;
                }

            }
            else if(0xC0 <= sid && sid <= 0xDF)
            {
                if(psm->guesst_video_sream == 1)
                {
                    psm->streams[psm->stream_count].codecid = PSI_STREAM_AAC;
                    findGuesstAudioSream = 1;
                }
            }
            if(findGuesstVideoSream == 1)
            {
                psm->guesst_video_sream = 1;
                return &psm->streams[psm->stream_count++];
            }
            else if(findGuesstAudioSream == 1)
            {
                psm->guesst_audio_sream = 1;
                return &psm->streams[psm->stream_count++];
            }
            else
            {
                 return NULL;
            }

        }
    }
#endif


    return NULL;
}

static int pes_packet_read(struct ps_demuxer_t *ps, const uint8_t* data, size_t bytes)
{
    int r;
    size_t n;
    size_t i = 0;
    size_t j = 0;
    size_t pes_packet_length;
    struct pes_t* pes;

    // MPEG_program_end_code = 0x000001B9
    for (i = 0; i + 5 < bytes && 0x00 == data[i] && 0x00 == data[i + 1] && 0x01 == data[i + 2]
        && PES_SID_END != data[i + 3]
        && PES_SID_START != data[i + 3];
        i += pes_packet_length + 6) 
    {
        pes_packet_length = (data[i + 4] << 8) | data[i + 5];
#if defined(MPEG_ZERO_PAYLOAD_LENGTH)
        // fix H.264/H.265 ps payload zero-length
        if (0 == pes_packet_length && 0xE0 <= data[i + 3] && data[i + 3] <= 0xEF)
        {
            pes_packet_length = ps_demuxer_find_pes_start(data + i + 6, bytes - i - 6);
        }
#endif

        //assert(i + 6 + pes_packet_length <= bytes);
        if (i + 6 + pes_packet_length > bytes)
            return i; // need more data

        // stream id
        switch (data[i+3])
        {
        case PES_SID_PSM:
            n = ps->psm.stream_count;
            j = psm_read(&ps->psm, data + i, pes_packet_length + 6);
            assert(j == pes_packet_length + 6);
            if ((n != ps->psm.stream_count) || (ps->psm.find_psm == 0 && ps->psm.guesst_video_sream == 1))
            {
                ps_demuxer_notify(ps); // TODO: check psm stream sid
            }
            ps->psm.find_psm = 1;
            break;

        case PES_SID_PSD:
            j = psd_read(&ps->psd, data + i, pes_packet_length + 6);
            assert(j == pes_packet_length + 6);
            break;

        case PES_SID_PRIVATE_2:
        case PES_SID_ECM:
        case PES_SID_EMM:
        case PES_SID_DSMCC:
        case PES_SID_H222_E:
            // stream data
            break;

        case PES_SID_PADDING:
            // padding
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
#if defined(MPEG_GUESS_STREAM)
            if(ps->psm.find_psm == 0)
            {
                n = ps->psm.stream_count;
            }
#endif
            pes = psm_fetch(&ps->psm, data[i+3],data,bytes);
            if (NULL == pes)
                continue;
#if defined(MPEG_GUESS_STREAM)
            if(ps->psm.find_psm == 0)
            {
                if (n != ps->psm.stream_count)
                {
                    ps_demuxer_notify(ps);
                }
            }
#endif

            assert(PES_SID_END != data[i + 3]);
			if (ps->pkhd.mpeg2)
				j = pes_read_header(pes, data + i, pes_packet_length + 6);
			else
				j = pes_read_mpeg1_header(pes, data + i, pes_packet_length + 6);

            if (j > 0)
            {
                r = pes_packet(&pes->pkt, pes, data + i + j, pes_packet_length + 6 - j, ps->start, ps_demuxer_onpes, ps);
                ps->start = 0; // clear start flag
                if (0 != r)
                    return r;
            }

            break;
        }

        if (0 == j)
            return i + 4; // invalid data, skip start code
    }

    return i;
}

int ps_demuxer_input(struct ps_demuxer_t* ps, const uint8_t* data, size_t bytes)
{
    int n;
	size_t i;
    
    for (i = ps_demuxer_find_startcode(data, bytes); data && i + 3 < bytes; i += ps_demuxer_find_startcode(data + i, bytes - i))
    {
        // fix HIK H.265: 00 00 01 BA 00 00 01 E0 ...
        if (i + 6 < bytes && 00 == data[i + 4] && 00 == data[i + 5] && 01 == data[i + 6])
        {
            i += 4;
            continue;
        }

        switch (data[i + 3])
        {
        case PES_SID_START:
            ps->start = 1;
            n = (int)pack_header_read(&ps->pkhd, data + i, bytes - i);
            break;
            
        case PES_SID_SYS:
            n = (int)system_header_read(&ps->system, data + i, bytes - i);
            break;
            
        case PES_SID_END:
            n = 4;
            break;
                
        default:
            n = pes_packet_read(ps, data + i, bytes - i);
            break;
        }

        if (n < 0)
            return n;

        assert(i + n <= bytes);
        if (0 == n || i + n > bytes)
            break;
        
        i += n;
    }

	return (int)i;
}

struct ps_demuxer_t* ps_demuxer_create(ps_demuxer_onpacket onpacket, void* param)
{
	struct ps_demuxer_t* ps;
	ps = calloc(1, sizeof(struct ps_demuxer_t));
	if(!ps)
		return NULL;

	ps->pkhd.mpeg2 = 1;
    ps->onpacket = onpacket;
	ps->param = param;
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
