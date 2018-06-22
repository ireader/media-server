// ITU-T H.222.0(06/2012)
// Information technology ¨C Generic coding of moving pictures and associated audio information: Systems
// 2.5.3.1 Program stream(p74)

#include "mpeg-ts-proto.h"
#include "mpeg-ps-proto.h"
#include "mpeg-pes-proto.h"
#include "mpeg-util.h"
#include "mpeg-ps.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define MAX_PES_HEADER	1024	// pack_header + system_header + psm
#define MAX_PES_PACKET	0xFFFF	// 64k pes data

struct ps_muxer_t
{
    struct psm_t psm; 
    struct ps_pack_header_t pack;
    struct ps_system_header_t system;
    
    int h264_h265_with_aud;
	unsigned int psm_period;
	unsigned int scr_period;

	struct ps_muxer_func_t func;
	void* param;

//	uint8_t packet[MAX_PACKET_SIZE];
};

static struct pes_t* ps_stream_find(struct ps_muxer_t *ps, int streamid)
{
    size_t i;
    for (i = 0; i < ps->psm.stream_count; i++)
    {
        if (streamid == ps->psm.streams[i].sid)
            return &ps->psm.streams[i];
    }
    return NULL;
}

int ps_muxer_input(struct ps_muxer_t* ps, int streamid, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes)
{
	int first;
	size_t i, n, sz;
	uint8_t *packet;
    struct pes_t* stream;
    const uint8_t* payload;

	i = 0;
	first = 1;
	payload = (const uint8_t*)data;

    stream = ps_stream_find(ps, streamid);
    if (NULL == stream) return -1; // not found
    stream->data_alignment_indicator = (flags & MPEG_FLAG_IDR_FRAME) ? 1 : 0; // idr frame
    stream->pts = pts;
    stream->dts = dts;

    ps->h264_h265_with_aud = (flags & MPEG_FLAG_H264_H265_WITH_AUD) ? 1 : 0;

	// TODO: 
	// 1. update packet header program_mux_rate
	// 2. update system header rate_bound

	// alloc once (include Multi-PES packet)
	sz = bytes + MAX_PES_HEADER + (bytes/MAX_PES_PACKET+1) * 64; // 64 = 0x000001 + stream_id + PES_packet_length + other
	packet = ps->func.alloc(ps->param, sz);
	if(!packet) return ENOMEM;

	// write pack_header(p74)
	// 2.7.1 Frequency of coding the system clock reference
	// http://www.bretl.com/mpeghtml/SCR.HTM
	//the maximum allowed interval between SCRs is 700ms 
	//ps->pack.system_clock_reference_base = (dts-3600) % (((int64_t)1)<<33);
	ps->pack.system_clock_reference_base = dts - 3600;
	ps->pack.system_clock_reference_extension = 0;
	ps->pack.program_mux_rate = 6106;
	i += pack_header_write(&ps->pack, packet + i);

	// write system_header(p76)
	if(0 == (ps->psm_period % 30))
		i += system_header_write(&ps->system, packet + i);

	// write program_stream_map(p79)
	if(0 == (ps->psm_period % 30))
		i += psm_write(&ps->psm, packet + i);

	// check packet size
	assert(i < MAX_PES_HEADER);

	// write data
	while(bytes > 0)
	{
		uint8_t *p;
		uint8_t *pes = packet + i;
		
		p = pes + pes_write_header(stream, pes, sz - i);
		assert(p - pes < 64);

		if(first)
		{
			if (PSI_STREAM_H264 == stream->codecid && !ps->h264_h265_with_aud)
			{
				// 2.14 Carriage of Rec. ITU-T H.264 | ISO/IEC 14496-10 video
				// Each AVC access unit shall contain an access unit delimiter NAL Unit
				nbo_w32(p, 0x00000001);
				p[4] = 0x09; // AUD
				p[5] = 0xE0; // any slice type (0xe) + rbsp stop one bit
				p += 6;
			}
			else if (PSI_STREAM_H265 == stream->codecid && !ps->h264_h265_with_aud)
			{
				// 2.17 Carriage of HEVC
				// Each HEVC access unit shall contain an access unit delimiter NAL unit.
				nbo_w32(p, 0x00000001);
				p[4] = 0x46; // 35-AUD_NUT
				p[5] = 01;
				p[6] = 0x50; // B&P&I (0x2) + rbsp stop one bit
				p += 7;
			}
		}

		// PES_packet_length = PES-Header + Payload-Size
		// A value of 0 indicates that the PES packet length is neither specified nor bounded 
		// and is allowed only in PES packets whose payload consists of bytes from a 
		// video elementary stream contained in transport stream packets
		if((p - pes - 6) + bytes > MAX_PES_PACKET)
		{
			nbo_w16(pes + 4, MAX_PES_PACKET);
			n = MAX_PES_PACKET - (p - pes - 6);
		}
		else
		{
			nbo_w16(pes + 4, (uint16_t)((p - pes - 6) + bytes));
			n = bytes;
		}

		memcpy(p, payload, n);
		payload += n;
		bytes -= n;

		// notify packet already
		i += n + (p - pes);

//		i = 0; // clear value, the next pes packet don't need pack_header
		first = 0; // clear first packet flag
		pts = dts = 0; // only first packet write PTS/DTS
	}

	assert(i < sz);
	ps->func.write(ps->param, stream->sid, packet, i);
	ps->func.free(ps->param, packet);

	++ps->psm_period;
	return 0;
}

struct ps_muxer_t* ps_muxer_create(const struct ps_muxer_func_t *func, void* param)
{
	struct ps_muxer_t *ps = NULL;

	assert(func);
	ps = (struct ps_muxer_t *)calloc(1, sizeof(struct ps_muxer_t));
	if(!ps)
		return NULL;

	memcpy(&ps->func, func, sizeof(ps->func));
	ps->param = param;

	ps->system.rate_bound = 26234; //10493600~10mbps(50BPS * 8 = 400bps)
//	ps->system.audio_bound = 1; // [0,32] max active audio streams
//	ps->system.video_bound = 1; // [0,16] max active video streams
	ps->system.fixed_flag = 0; // 1-fixed bitrate, 0-variable bitrate
	ps->system.CSPS_flag = 0; // meets the constraints defined in 2.7.9.
	ps->system.packet_rate_restriction_flag = 0; // dependence CSPS_flag
	ps->system.system_audio_lock_flag = 0; // all audio stream sampling rate is constant
	ps->system.system_video_lock_flag = 0; // all video stream frequency is constant

	//ps->psm.ver = 1;
	//ps->psm.stream_count = 2;
	//ps->psm.streams[0].element_stream_id = PES_SID_VIDEO;
	//ps->psm.streams[0].stream_type = PSI_STREAM_H264;
	//ps->psm.streams[1].element_stream_id = PES_SID_AUDIO;
	//ps->psm.streams[1].stream_type = PSI_STREAM_AAC;

	return ps;
}

int ps_muxer_destroy(struct ps_muxer_t* ps)
{
    size_t i;
    for (i = 0; i < ps->psm.stream_count; i++)
    {
        if (ps->psm.streams[i].esinfo)
        {
            free(ps->psm.streams[i].esinfo);
            ps->psm.streams[i].esinfo = NULL;
        }
    }

	free(ps);
	return 0;
}

int ps_muxer_add_stream(struct ps_muxer_t* ps, int codecid, const void* extradata, size_t bytes)
{
    struct psm_t *psm;
    struct pes_t *pes;

	assert(bytes < 512);
	if(!ps || ps->psm.stream_count >= sizeof(ps->psm.streams)/sizeof(ps->psm.streams[0]))
	{
		assert(0);
		return -1;
	}

	psm = &ps->psm;
    pes = &psm->streams[psm->stream_count];

	if (mpeg_stream_type_video(codecid))
    {
        pes->sid = (uint8_t)(PES_SID_VIDEO + ps->system.video_bound);

        assert(ps->system.video_bound + 1 < 16);
        ++ps->system.video_bound; // [0,16] max active video streams
        ps->system.streams[ps->system.stream_count].buffer_bound_scale = 1;
        /* FIXME -- VCD uses 46, SVCD uses 230, ffmpeg has 230 with a note that it is small */
        ps->system.streams[ps->system.stream_count].buffer_size_bound = 400 /* 8191-13 bits max value */;
    }
    else if (mpeg_stream_type_audio(codecid))
    {
        pes->sid = (uint8_t)(PES_SID_AUDIO + ps->system.audio_bound);

        assert(ps->system.audio_bound + 1 < 32);
        ++ps->system.audio_bound; // [0,32] max active audio streams
        ps->system.streams[ps->system.stream_count].buffer_bound_scale = 0;
        /* This value HAS to be used for VCD (see VCD standard, p. IV-7).
        * Right now it is also used for everything else. */
        ps->system.streams[ps->system.stream_count].buffer_size_bound = 32 /* 4 * 1024 / 128 */;
    }
    else
    {
        assert(0);
		return -1;
	}

    if (bytes > 0)
    {
        pes->esinfo = (uint8_t*)malloc(bytes);
        if (!pes->esinfo)
            return -1;
        memcpy(pes->esinfo, extradata, bytes);
        pes->esinfo_len = (uint16_t)bytes;
    }

    assert(psm->stream_count == ps->system.stream_count);
    ps->system.streams[ps->system.stream_count].stream_id = pes->sid;
	++ps->system.stream_count;

    pes->codecid = (uint8_t)codecid;
	++psm->stream_count;
	++psm->ver;

	ps->psm_period = 0; // immediate update psm
	return pes->sid;
}
