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
#include <memory.h>
#include <assert.h>

#define MAX_PES_HEADER	1024	// pack_header + system_header + psm
#define MAX_PES_PACKET	0xFFFF	// 64k pes data

typedef struct _mpeg_ps_enc_context_t
{
	ps_packet_header_t packhd;
	ps_system_header_t syshd;
	psm_t psm;

	unsigned int psm_period;
	unsigned int scr_period;

	struct mpeg_ps_func_t func;
	void* param;

//	uint8_t packet[MAX_PACKET_SIZE];

} mpeg_ps_enc_context_t;

static uint8_t ps_stream_find(mpeg_ps_enc_context_t *psctx, int avtype)
{
	size_t i;
	for(i = 0; i < psctx->psm.stream_count; i++)
	{
		if(avtype == psctx->psm.streams[i].avtype)
			return psctx->psm.streams[i].pesid;
	}
	return 0;
}

static size_t ps_packet_header_write(const ps_packet_header_t *packethd, uint8_t *data)
{
	// pack_start_code
	nbo_w32(data, 0x000001BA);

	// 33-system_clock_reference_base + 9-system_clock_reference_extension
	// '01xxx1xx xxxxxxxx xxxxx1xx xxxxxxxx xxxxx1xx xxxxxxx1'
	data[4] = 0x44 | (((packethd->system_clock_reference_base >> 30) & 0x07) << 3) | ((packethd->system_clock_reference_base >> 28) & 0x03);
	data[5] = ((packethd->system_clock_reference_base >> 20) & 0xFF);
	data[6] = 0x04 | (((packethd->system_clock_reference_base >> 15) & 0x1F) << 3) | ((packethd->system_clock_reference_base >> 13) & 0x03);
	data[7] = ((packethd->system_clock_reference_base >> 5) & 0xFF);
	data[8] = 0x04 | ((packethd->system_clock_reference_base & 0x1F) << 3) | ((packethd->system_clock_reference_extension >> 7) & 0x03);
	data[9] = 0x01 | ((packethd->system_clock_reference_extension & 0x7F) << 1);

	// program_mux_rate
	// 'xxxxxxxx xxxxxxxx xxxxxx11'
	data[10] = (uint8_t)(packethd->program_mux_rate >> 14);
	data[11] = (uint8_t)(packethd->program_mux_rate >> 6);
	data[12] = (uint8_t)(0x03 | ((packethd->program_mux_rate & 0x3F) << 2));

	// stuffing length
	// '00000xxx'
	data[13] = 0;

	return 14;
}

static size_t ps_system_header_write(const ps_system_header_t *syshd, uint8_t *data)
{
	size_t i, j;

	// system_header_start_code
	nbo_w32(data, 0x000001BB);

	// header length
	//put16(data + 4, 6 + syshd->stream_count*3);

	// rate_bound
	// 1xxxxxxx xxxxxxxx xxxxxxx1
	data[6] = 0x80 | ((syshd->rate_bound >> 15) & 0x7F);
	data[7] = (syshd->rate_bound >> 7) & 0xFF;
	data[8] = 0x01 | ((syshd->rate_bound & 0x7F) << 1);

	// 6-audio_bound + 1-fixed_flag + 1-CSPS_flag
	data[9] = ((syshd->audio_bound & 0x3F) << 2) | ((syshd->fixed_flag & 0x01) << 1) | (syshd->CSPS_flag & 0x01);

	// 1-system_audio_lock_flag + 1-system_video_lock_flag + 1-maker + 5-video_bound
	data[10] = 0x20 | ((syshd->system_audio_lock_flag & 0x01) << 7) | ((syshd->video_bound & 0x01) << 6) | (syshd->video_bound & 0x1F);

	// 1-packet_rate_restriction_flag + 7-reserved
	data[11] = (syshd->packet_rate_restriction_flag & 0x01) << 7;

	i = 12;
	for(j = 0; j < syshd->stream_count; j++)
	{
		data[i++] = (uint8_t)syshd->streams[j].stream_id;
		if(PES_SID_EXTENSION == syshd->streams[j].stream_id) // '10110111'
		{
			data[i++] = 0xD0; // '11000000'
			data[i++] = syshd->streams[j].stream_extid & 0x7F; // '0xxxxxxx'
			data[i++] = 0xB6; // '10110110'
		}

		// '11' + 1-P-STD_buffer_bound_scale + 13-P-STD_buffer_size_bound
		// '11xxxxxx xxxxxxxx'
		data[i++] = 0xC0 | ((syshd->streams[j].buffer_bound_scale & 0x01) << 5) | ((syshd->streams[j].buffer_size_bound >> 8) & 0x1F);
		data[i++] = syshd->streams[j].buffer_size_bound & 0xFF;
	}

	// header length
	nbo_w16(data + 4, (uint16_t)(i-6));
	return i;
}

int mpeg_ps_write(void* ps, int avtype, int64_t pts, int64_t dts, const void* data, size_t bytes)
{
	int first;
	size_t i, n, sz;
	uint8_t *packet;
	const uint8_t* payload;
	mpeg_ps_enc_context_t *psctx;

	i = 0;
	first = 1;
	psctx = (mpeg_ps_enc_context_t*)ps;
	payload = (const uint8_t*)data;

	// TODO: 
	// 1. update packet header program_mux_rate
	// 2. update system header rate_bound

	// alloc once (include Multi-PES packet)
	sz = bytes + MAX_PES_HEADER + (bytes/MAX_PES_PACKET+1) * 64; // 64 = 0x000001 + stream_id + PES_packet_length + other
	packet = psctx->func.alloc(psctx->param, sz);
	if(!packet) return ENOMEM;

	// write pack_header(p74)
	// 2.7.1 Frequency of coding the system clock reference
	// http://www.bretl.com/mpeghtml/SCR.HTM
	//the maximum allowed interval between SCRs is 700ms 
	//psctx->packhd.system_clock_reference_base = (dts-3600) % (((int64_t)1)<<33);
	psctx->packhd.system_clock_reference_base = dts - 3600;
	psctx->packhd.system_clock_reference_extension = 0;
	psctx->packhd.program_mux_rate = 6106;
	i += ps_packet_header_write(&psctx->packhd, packet + i);

	// write system_header(p76)
	if(0 == (psctx->psm_period % 30))
		i += ps_system_header_write(&psctx->syshd, packet + i);

	// write program_stream_map(p79)
	if(0 == (psctx->psm_period % 30))
		i += psm_write(&psctx->psm, packet + i);

	// check packet size
	assert(i < MAX_PES_HEADER);

	// write data
	while(bytes > 0)
	{
		uint8_t *p;
		uint8_t *pes = packet + i;
		uint8_t streamId;

		streamId = ps_stream_find(psctx, avtype);
		assert(PES_SID_VIDEO==streamId || PES_SID_AUDIO==streamId);
		p = pes + pes_write_header(pts, dts, streamId, pes);
		assert(p - pes < 64);

		if(first && PSI_STREAM_H264 == avtype && 0 == find_h264_access_unit_delimiter(payload, bytes))
		{
			// 2.14 Carriage of Rec. ITU-T H.264 | ISO/IEC 14496-10 video
			// Each AVC access unit shall contain an access unit delimiter NAL Unit
			nbo_w32(p, 0x00000001);
			p[4] = 0x09; // AUD
			p[5] = 0xE0; // any slice type (0xe) + rbsp stop one bit
			p += 6;
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
	psctx->func.write(psctx->param, avtype, packet, i);
	psctx->func.free(psctx->param, packet);

	++psctx->psm_period;
	return 0;
}

void* mpeg_ps_create(const struct mpeg_ps_func_t *func, void* param)
{
	mpeg_ps_enc_context_t *psctx = NULL;

	assert(func);
	psctx = (mpeg_ps_enc_context_t *)malloc(sizeof(mpeg_ps_enc_context_t));
	if(!psctx)
		return NULL;

	memset(psctx, 0, sizeof(mpeg_ps_enc_context_t));
	memcpy(&psctx->func, func, sizeof(psctx->func));
	psctx->param = param;

	psctx->syshd.rate_bound = 26234; //10493600~10mbps(50BPS * 8 = 400bps)
//	psctx->syshd.audio_bound = 1; // [0,32] max active audio streams
//	psctx->syshd.video_bound = 1; // [0,16] max active video streams
	psctx->syshd.fixed_flag = 0; // 1-fixed bitrate, 0-variable bitrate
	psctx->syshd.CSPS_flag = 0; // meets the constraints defined in 2.7.9.
	psctx->syshd.packet_rate_restriction_flag = 0; // dependence CSPS_flag
	psctx->syshd.system_audio_lock_flag = 1; // all audio stream sampling rate is constant
	psctx->syshd.system_video_lock_flag = 1; // all video stream frequency is constant

	//psctx->psm.ver = 1;
	//psctx->psm.stream_count = 2;
	//psctx->psm.streams[0].element_stream_id = PES_SID_VIDEO;
	//psctx->psm.streams[0].stream_type = PSI_STREAM_H264;
	//psctx->psm.streams[1].element_stream_id = PES_SID_AUDIO;
	//psctx->psm.streams[1].stream_type = PSI_STREAM_AAC;

	return psctx;
}

int mpeg_ps_destroy(void* ps)
{
	mpeg_ps_enc_context_t *psctx;

	psctx = (mpeg_ps_enc_context_t*)ps;
	free(psctx);
	return 0;
}

int mpeg_ps_add_stream(void* ps, int avtype, const void* info, size_t bytes)
{
	mpeg_ps_enc_context_t* psctx;
	psm_t *psm;

	assert(bytes < 512);
	psctx = (mpeg_ps_enc_context_t*)ps;
	if(!psctx || psctx->psm.stream_count + 1 >= NSTREAM)
	{
		assert(0);
		return -1;
	}

	psm = &psctx->psm;
	if(bytes > 0)
	{
		psm->streams[psm->stream_count].esinfo = (uint8_t*)malloc(bytes);
		if(psm->streams[psm->stream_count].esinfo)
		{
			memcpy(psm->streams[psm->stream_count].esinfo, info, bytes);
			psm->streams[psm->stream_count].esinfo_len = (uint16_t)bytes;
		}
	}

	switch(avtype)
	{
	case PSI_STREAM_MPEG1:
	case PSI_STREAM_MPEG2:
	case PSI_STREAM_MPEG4:
	case PSI_STREAM_H264:
	case PSI_STREAM_VIDEO_VC1:
	case PSI_STREAM_VIDEO_DIRAC:
	case PSI_STREAM_VIDEO_SVAC:
		psm->streams[psm->stream_count].pesid = (uint8_t)(PES_SID_VIDEO + psctx->syshd.video_bound);

		assert(psctx->syshd.video_bound + 1 < 16);
		++psctx->syshd.video_bound; // [0,16] max active video streams
		psctx->syshd.streams[psctx->syshd.stream_count].buffer_bound_scale = 1;
		psctx->syshd.streams[psctx->syshd.stream_count].buffer_size_bound = 232;
		break;

	case PSI_STREAM_AUDIO_MPEG1:
	case PSI_STREAM_MP3:
	case PSI_STREAM_AAC:
	case PSI_STREAM_MPEG4_AAC_LATM:
	case PSI_STREAM_AUDIO_AC3:
	case PSI_STREAM_AUDIO_DTS:
	case PSI_STREAM_AUDIO_SVAC:
	case PSI_STREAM_AUDIO_G711:
	case PSI_STREAM_AUDIO_G722:
	case PSI_STREAM_AUDIO_G723:
	case PSI_STREAM_AUDIO_G729:
		psm->streams[psm->stream_count].pesid = (uint8_t)(PES_SID_AUDIO + psctx->syshd.audio_bound);

		assert(psctx->syshd.audio_bound + 1 < 32);
		++psctx->syshd.audio_bound; // [0,32] max active audio streams
		psctx->syshd.streams[psctx->syshd.stream_count].buffer_bound_scale = 0;
		psctx->syshd.streams[psctx->syshd.stream_count].buffer_size_bound = 32;
		break;

	default:
		assert(0);
		return -1;
	}

	psctx->syshd.streams[psctx->syshd.stream_count].stream_id = psm->streams[psm->stream_count].pesid;
	++psctx->syshd.stream_count;

	psm->streams[psm->stream_count].avtype = (uint8_t)avtype;
	++psm->stream_count;
	++psm->ver;

	assert(psm->stream_count == psctx->syshd.stream_count);
	psctx->psm_period = 0; // immediate update psm
	return 0;
}
