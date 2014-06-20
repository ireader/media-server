// ITU-T H.222.0(06/2012)
// Information technology ¨C Generic coding of moving pictures and associated audio information: Systems
//
#include "mpeg-ts.h"
#include "mpeg-ts-proto.h"
#include "crc32.h"
#include <stdlib.h>
#include <memory.h>
#include <assert.h>

#define N_MPEG_TS_STREAM	8
#define TS_PACKET_SIZE		188

#define inline

typedef struct _mpeg_ts_enc_context_t
{
	ts_pat_t pat;

	unsigned int pat_period;

	mpeg_ts_cbwrite write;
	void* param;
} mpeg_ts_enc_context_t;

static inline void put16(unsigned char *ptr, int val)
{
	ptr[0] = (val >> 8) & 0xFF;
	ptr[1] = val & 0xFF;
}

static inline void put32(unsigned char *ptr, int val)
{
	ptr[0] = (val >> 24) & 0xFF;
	ptr[1] = (val >> 16) & 0xFF;
	ptr[2] = (val >> 8) & 0xFF;
	ptr[3] = val & 0xFF;
}

static inline unsigned char h264_type(const unsigned char *data, int bytes)
{
	int i;
	for(i = 0; i + 3 < bytes; i++)
	{
		if(0x00 == data[i] && 0x00 == data[i+1] && 0x01 == data[i+2])
			return data[i+3] & 0x1f;
	}

	return 0x00;
}

static int ts_write_pat(ts_pat_t *pat, unsigned char *data)
{
	// 2.4.4.3 Program association table
	// Table 2-30

	int i = 0;
	int section_length = 0;

	section_length = pat->pmt_count * 4 + 5 + 4; // 5 bytes remain header and 4 bytes crc32

	// shall not exceed 1021 (0x3FD).
	assert(section_length <= 1021);
	assert(section_length <= TS_PACKET_SIZE - 7);

	data[0] = 0x00;	// program association table

	// section_syntax_indicator = '1'
	// '0'
	// reserved '11'
	put16(data + 1, 0xb000 | section_length);

	// transport_stream_id
	put16(data + 3, pat->tsid);

	// reserved '11'
	// version_number 'xxxxx'
	// current_next_indicator '1'
	data[5] = (unsigned char)(0xC1 | (pat->ver << 1));

	// section_number/last_section_number
	data[6] = 0x00;
	data[7] = 0x00;

	for(i = 0; i < pat->pmt_count; i++)
	{
		put16(data + 8 + i * 4 + 0, pat->pmt[i].pn);
		put16(data + 8 + i * 4 + 2, 0xE000 | pat->pmt[i].pid);
	}

	// crc32
	i = crc32(-1, data, section_length-1);
	put32(data + section_length - 1, i);

	return section_length + 3; // total length
}

static int ts_write_pmt(ts_pmt_t *pmt, unsigned char *data)
{
	// 2.4.4.8 Program map table
	// Table 2-33

	int i = 0;
	int section_length = 0;
	unsigned char *p = NULL;

	data[0] = 0x02;	// program map table

	// skip section_length

	// program_number
	put16(data + 3, pmt->pn);

	// reserved '11'
	// version_number 'xxxxx'
	// current_next_indicator '1'
	data[4] = (unsigned char)(0xC1 | (pmt->ver << 1));

	// section_number/last_section_number
	data[5] = 0x00;
	data[6] = 0x00;

	// reserved '111'
	// PCR_PID 15-bits 0x1FFF
	put16(data + 7, 0xE000 | pmt->PCR_PID);

	// reserved '1111'
	// program_info_lengt 12-bits
	put16(data + 9, 0xF000 | pmt->program_info_length);
	if(pmt->program_info_length > 0)
	{
		// fill program info
		assert(pmt->program_info);
		memcpy(data + 11, pmt->program_info, pmt->program_info_length);
	}

	// streams
	p = data + 11 + pmt->program_info_length;
	for(i = 0; i < pmt->stream_count; i++)
	{
		// stream_type
		*p = (unsigned char)pmt->streams[i].sid;

		// reserved '111'
		// elementary_PID 13-bits
		put16(p + 1, 0xE000 | pmt->streams[i].pid);

		// reserved '1111'
		// ES_info_lengt 12-bits
		put16(p + 3, 0xF000 | pmt->streams[i].es_info_length);

		// fill elementary stream info
		if(pmt->streams[i].es_info_length > 0)
		{
			assert(pmt->streams[i].es_info);
			memcpy(p + 5, pmt->streams[i].es_info, pmt->streams[i].es_info_length);
		}

		p += 5 + pmt->streams[i].es_info_length;
	}

	// section_length
	section_length = p + 4 - (data + 3); // 4 bytes crc32
	assert(section_length <= 1021); // shall not exceed 1021 (0x3FD).
	assert(section_length <= TS_PACKET_SIZE - 7);
	// section_syntax_indicator '1'
	// '0'
	// reserved '11'
	put16(data + 1, 0xb000 | section_length); 

	// crc32
	i = crc32(-1, data, p-data);
	put32(p, i);

	return p - data + 4; // total length
}

static int ts_write_pes_header(int64_t pts, int64_t dts, int streamId, unsigned char* data)
{
	int len = 0;
	int flags = 0x00;
	unsigned char *p = NULL;

	// packet_start_code_prefix 0x000001
	data[0] = 0x00;
	data[1] = 0x00;
	data[2] = 0x01;

	// stream id
	// Table 2-22 ¨C Stream_id assignments
	if(STREAM_VIDEO_H264==streamId || STREAM_VIDEO_MPEG4==streamId || STREAM_VIDEO_MPEG2==streamId || STREAM_VIDEO_MPEG1==streamId || STREAM_VIDEO_VC1==streamId)
	{
		// Rec. ITU-T H.262 | ISO/IEC 13818-2, ISO/IEC 11172-2, ISO/IEC 14496-2 
		// or Rec. ITU-T H.264 | ISO/IEC 14496-10 video stream number
		data[3] = PES_VIDEO_STREAM;
	}
	else if(STREAM_AUDIO_AAC==streamId || STREAM_AUDIO_AAC_LATM==streamId || STREAM_AUDIO_MPEG2==streamId)
	{
		// ISO/IEC 13818-3 or ISO/IEC 11172-3 or ISO/IEC 13818-7 or ISO/IEC 14496-3
		// audio stream number
		data[3] = PES_AUDIO_STREAM;
	}
	else
	{
		// private_stream_1
		data[3] = PES_PRIVATE_STREAM_1;
	}

	// skip PES_packet_length
	//data[4] = 0x00;
	//data[5] = 0x00;

	// '10'
	// PES_scrambling_control '00'
	// PES_priority '0'
	// data_alignment_indicator '0' ('1' for subtitle data)
	// copyright '0'
	// original_or_copy '0'
	//data[6] = SUBTITLE ? 0x84 : 0x80;
	data[6] = 0x80;

	// PTS_DTS_flag 'xx'
	// ESCR_flag '0'
	// ES_rate_flag '0'
	// DSM_trick_mode_flag '0'
	// additional_copy_info_flag '0'
	// PES_CRC_flag '0'
	// PES_extension_flag '0'
	if(pts)
	{
		flags |= 0x80;
		len += 5;
	}
	if(dts && dts != pts)
	{
		flags |= 0x40;
		len += 5;
	}
	data[7] = (unsigned char)flags;

	// PES_header_data_length
	data[8] = (unsigned char)len;

	p = data + 9;
	if(flags & 0x80)
	{
		*p++ = (unsigned char)((flags>>6) | ((pts >> 29) & 0x0E));
		*p++ = (pts >> 22) & 0xFF;
		*p++ = 0x01 | ((pts >> 14) & 0xFE);
		*p++ = (pts >> 7) & 0xFF;;
		*p++ = 0x01 | ((pts << 1) & 0xFE);
	}

	if(flags & 0x40)
	{
		*p++ = 0x01 | ((dts >> 29) & 0x0E);
		*p++ = (dts >> 22) & 0xFF;
		*p++ = 0x01 | ((dts >> 14) & 0xFE);
		*p++ = (dts >> 7) & 0xFF;;
		*p++ = 0x01 | ((dts << 1) & 0xFE);
	}

	return p - data;
}

static int ts_write_pes(mpeg_ts_enc_context_t *tsctx, int pid, int cc, int64_t pts, int64_t dts, int streamId, const unsigned char* payload, int bytes)
{
	// 2.4.3.6 PES packet
	// Table 2-21

	int len = 0;
	int flags = 0;
	int start = 1; // first packet
	unsigned char *p = NULL;
	unsigned char data[TS_PACKET_SIZE];

	while(bytes > 0)
	{
		p = data + 4;

		// PES header
		if(start)
		{
			p += ts_write_pes_header(pts, dts, streamId, p);

			if(STREAM_VIDEO_H264 == streamId && 0x09 != h264_type(payload, bytes))
			{
				// 2.14 Carriage of Rec. ITU-T H.264 | ISO/IEC 14496-10 video
				// Each AVC access unit shall contain an access unit delimiter NAL Unit
				put32(p, 0x00000001);
				p[4] = 0x09; // AUD
				p[5] = 0xE0; // any slice type (0xe) + rbsp stop one bit
				p += 6;
			}

			// PES_packet_length = PES-Header + Payload-Size
			// A value of 0 indicates that the PES packet length is neither specified nor bounded 
			// and is allowed only in PES packets whose payload consists of bytes from a 
			// video elementary stream contained in transport stream packets
			if((p - data - 4 - 6) + bytes > 0xFFFF)
				put32(data + 4 + 4, 0); // 2.4.3.7 PES packet => PES_packet_length
			else
				put16(data + 4 + 4, (p - data - 4 - 6) + bytes);
		}

		// TS Header
		data[0] = 0x47;	// sync_byte
		data[1] = 0x40 | (pid & 0x1F);
		data[2] = pid & 0xFF;
		data[3] = 0x10 | cc;

		if(p - data + bytes >= TS_PACKET_SIZE)
		{
			len = TS_PACKET_SIZE - (p - data);
		}
		else
		{
			// adaptation
			data[3] |= 0x20;
			data[4] = TS_PACKET_SIZE - (p - data + bytes) - 1;
			data[5] = 0x00; // don't set any flag
			memset(data + 6, 0xFF, data[4] - 1);

			// adaption
			if(p - data > 4)
				memmove(data + 4 + data[4] + 1, data + 4, p - data - 4);

			p = data + (TS_PACKET_SIZE - bytes);
			len = bytes;
		}

		// payload
		memcpy(p, payload, len);

		payload += len;
		bytes -= len;
		start = 0;

		// send with TS-header
		tsctx->write(tsctx->param, data, TS_PACKET_SIZE);
	}
}

static void mpeg_ts_write_section_header(mpeg_ts_enc_context_t *ts, int pid, int cc, const void* payload, int len)
{
	unsigned char *p;
	unsigned char data[TS_PACKET_SIZE];

	assert(len < TS_PACKET_SIZE - 5); // TS-header + pointer

	// TS Payload
	memmove(data + TS_PACKET_SIZE - len, payload, len);

	// TS Header

	// sync_byte
	data[0] = 0x47;
	// transport_error_indicator = 0
	// payload_unit_start_indicator = 1
	// transport_priority = 0
	data[1] = 0x40 | (pid & 0x1F);
	data[2] = pid & 0xFF;
	// transport_scrambling_control = 0x00
	// adaptation_field_control = 0x11 adaptation and payload
	data[3] = 0x10 | cc;

	// Adaptation
	if(len < TS_PACKET_SIZE - 5)
	{
		data[3] |= 0x20; // with adaptation
		data[4] = TS_PACKET_SIZE - len - 5 - 1; // 4B-Header + 1B-pointer + 1B-self
		if(data[4] > 0)
		{
			// adaptation
			data[5] = 0;
			memset(data+6, 0xFF, data[4]-1);
		}
	}

	// pointer
	data[TS_PACKET_SIZE-len-1] = 0x00;

	ts->write(ts->param, data, TS_PACKET_SIZE);
}

int mpeg_ts_write(void* ts, int streamId, unsigned char* data, int bytes)
{
	int i, r, pid, *cc;
	int64_t pts, dts;
	mpeg_ts_enc_context_t *tsctx;	
	unsigned char payload[TS_PACKET_SIZE];

	tsctx = (mpeg_ts_enc_context_t*)ts;

	if(0 == tsctx->pat_period)
	{
		// PAT
		tsctx->pat.cc = (tsctx->pat.cc + 1) % 16;
		r = ts_write_pat(&tsctx->pat, payload);
		mpeg_ts_write_section_header(ts, 0x00, tsctx->pat.cc, payload, r); // PID = 0x00 program association table

		// PMT
		for(i = 0; i < tsctx->pat.pmt_count; i++)
		{
			tsctx->pat.pmt[i].cc = (tsctx->pat.pmt[i].cc + 1) % 16;
			r = ts_write_pmt(&tsctx->pat.pmt[i], payload);
			mpeg_ts_write_section_header(ts, tsctx->pat.pmt[i].pid, tsctx->pat.pmt[i].cc, payload, r);
		}
	}

	tsctx->pat_period = (tsctx->pat_period + 1) % 200;

	if(STREAM_VIDEO_H264 == streamId)
	{
		pid = tsctx->pat.pmt[0].streams[0].pid;
		cc = &tsctx->pat.pmt[0].streams[0].cc;
	}
	else if(STREAM_AUDIO_AAC == streamId)
	{
		pid = tsctx->pat.pmt[0].streams[1].pid;
		cc = &tsctx->pat.pmt[0].streams[1].cc;
	}
	else
	{
		assert(0);
	}

	*cc = (*cc + 1 ) % 16;
	ts_write_pes(tsctx, pid, *cc, pts, dts, streamId, data, bytes);
	return 0;
}

int mpeg_ts_write_stream(void* ts, int streamId, unsigned char* data, int bytes)
{
	return 0;
}

void* mpeg_ts_create(mpeg_ts_cbwrite func, void* param)
{
	mpeg_ts_enc_context_t *tsctx = NULL;

	assert(func);
	tsctx = (mpeg_ts_enc_context_t *)malloc(sizeof(mpeg_ts_enc_context_t) 
											+ sizeof(tsctx->pat.pmt[0])
											+ 2 * sizeof(tsctx->pat.pmt[0].streams[0]));
	if(!tsctx)
		return NULL;

	memset(tsctx, 0, sizeof(mpeg_ts_enc_context_t));
	tsctx->pat.tsid = 0;
	tsctx->pat.ver = 0;
	tsctx->pat.cc = -1; // +1 => 0

	tsctx->pat.pmt_count = 1; // only one program in ts
	tsctx->pat.pmt = tsctx + 1;
	tsctx->pat.pmt[0].pid = 1;
	tsctx->pat.pmt[0].pn = 0x40;
	tsctx->pat.pmt[0].ver = 0;
	tsctx->pat.pmt[0].cc = -1; // +1 => 0
	tsctx->pat.pmt[0].program_info_length = 0;
	tsctx->pat.pmt[0].PCR_PID = 0x42;

	tsctx->pat.pmt[0].stream_count = 2; // H.264 + AAC
	tsctx->pat.pmt[0].streams = tsctx->pat.pmt + 1;
	tsctx->pat.pmt[0].streams[0].pid = 0x42;
	tsctx->pat.pmt[0].streams[0].sid = STREAM_VIDEO_H264;
	tsctx->pat.pmt[0].streams[0].es_info_length = 0x00;
	tsctx->pat.pmt[0].streams[0].es_info = NULL;
	tsctx->pat.pmt[0].streams[1].pid = 0x43;
	tsctx->pat.pmt[0].streams[1].sid = STREAM_AUDIO_AAC;
	tsctx->pat.pmt[0].streams[1].es_info_length = 0x00;
	tsctx->pat.pmt[0].streams[1].es_info = NULL;

	tsctx->write = func;
	tsctx->param = param;
	return tsctx;
}

int mpeg_ts_destroy(void* ts)
{
	int i, j;
	mpeg_ts_enc_context_t *tsctx = NULL;
	tsctx = (mpeg_ts_enc_context_t*)ts;

	for(i = 0; i < tsctx->pat.pmt_count; i++)
	{
		for(j = 0; j < tsctx->pat.pmt[i].stream_count; j++)
		{
			if(tsctx->pat.pmt[i].streams[j].es_info)
				free(tsctx->pat.pmt[i].streams[j].es_info);
		}
	}

	free(tsctx);
	return 0;
}

//int mpeg_ts_add_stream(void* ts, int streamId)
//{
//	ts_pmt_t *pmt = NULL;
//
//	pmt = &((mpeg_ts_enc_context_t*)ts)->pmt;
//	if(pmt->stream_count + 1 >= N_MPEG_TS_STREAM)
//	{
//		assert(0);
//		return -1;
//	}
//
//	pmt->streams[pmt->stream_count].stream_type = streamId;
//	pmt->streams[pmt->stream_count].elementary_pid = 0x42 + pmt->stream_count;
//	pmt->streams[pmt->stream_count].es_info_length = 0;
//	pmt->streams[pmt->stream_count].es_info = NULL;
//	++pmt->stream_count;
//
//	//switch(streamId)
//	//{
//	//case STREAM_VIDEO_MPEG1:
//	//case STREAM_VIDEO_MPEG2:
//	//case STREAM_VIDEO_MPEG4:
//	//case STREAM_VIDEO_H264:
//	//case STREAM_VIDEO_VC1:
//	//case STREAM_VIDEO_DIRAC:
//	//	break;
//
//	//case STREAM_AUDIO_MPEG1:
//	//case STREAM_AUDIO_MPEG2:
//	//case STREAM_AUDIO_AAC:
//	//case STREAM_AUDIO_AC3:
//	//case STREAM_AUDIO_DTS:
//	//	break;
//
//	//case STREAM_PRIVATE_SECTION:
//	//case STREAM_PRIVATE_DATA:
//	//	break;
//
//	//default:
//	//	assert(0);
//	//	return -1;
//	//}
//
//	return 0;
//}
