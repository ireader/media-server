// ITU-T H.222.0(06/2012)
// Information technology ¨C Generic coding of moving pictures and associated audio information: Systems
//

#include <stdlib.h>
#include "mpeg-ts.h"
#include "mpeg-ts-proto.h"
#include "cstringext.h"
#include "h264-util.h"
#include "crc32.h"
#include <memory.h>
#include <string.h>
#include <assert.h>

#define N_MPEG_TS_STREAM	8
#define TS_PACKET_SIZE		188

typedef struct _mpeg_ts_enc_context_t
{
	ts_pat_t pat;

	unsigned int pat_period;
	unsigned int pcr_period;

	mpeg_ts_cbwrite write;
	void* param;
} mpeg_ts_enc_context_t;

inline void put16(uint8_t *ptr, uint32_t val)
{
	ptr[0] = (val >> 8) & 0xFF;
	ptr[1] = val & 0xFF;
}

inline void put32(uint8_t *ptr, uint32_t val)
{
	ptr[0] = (val >> 24) & 0xFF;
	ptr[1] = (val >> 16) & 0xFF;
	ptr[2] = (val >> 8) & 0xFF;
	ptr[3] = val & 0xFF;
}

inline void ts_write_pcr(uint8_t *ptr, int64_t pcr)
{
	int64_t pcr_base = pcr / 300;
	int64_t pcr_ext = pcr % 300;

	ptr[0] = (pcr_base >> 25) & 0xFF;
	ptr[1] = (pcr_base >> 17) & 0xFF;
	ptr[2] = (pcr_base >> 9) & 0xFF;
	ptr[3] = (pcr_base >> 1) & 0xFF;
	ptr[4] = ((pcr_base & 0x01) << 7) | 0x7E | ((pcr_ext>>8) & 0x01);
	ptr[5] = pcr_ext & 0xFF;
}

static size_t ts_write_pat(const ts_pat_t *pat, uint8_t *data)
{
	// 2.4.4.3 Program association table
	// Table 2-30

	uint32_t i = 0;
	uint32_t len = 0;
	uint32_t crc = 0;

	len = pat->pmt_count * 4 + 5 + 4; // 5 bytes remain header and 4 bytes crc32

	// shall not exceed 1021 (0x3FD).
	assert(len <= 1021);
	assert(len <= TS_PACKET_SIZE - 7);

	data[0] = 0x00;	// program association table

	// section_syntax_indicator = '1'
	// '0'
	// reserved '11'
	put16(data + 1, 0xb000 | len);

	// transport_stream_id
	put16(data + 3, pat->tsid);

	// reserved '11'
	// version_number 'xxxxx'
	// current_next_indicator '1'
	data[5] = (uint8_t)(0xC1 | (pat->ver << 1));

	// section_number/last_section_number
	data[6] = 0x00;
	data[7] = 0x00;

	for(i = 0; i < pat->pmt_count; i++)
	{
		put16(data + 8 + i * 4 + 0, pat->pmt[i].pn);
		put16(data + 8 + i * 4 + 2, 0xE000 | pat->pmt[i].pid);
	}

	// crc32
	crc = crc32(0xffffffff, data, len-1);
	//put32(data + section_length - 1, crc);
	data[len - 1 + 3] = (crc >> 24) & 0xFF;
	data[len - 1 + 2] = (crc >> 16) & 0xFF;
	data[len - 1 + 1] = (crc >> 8) & 0xFF;
	data[len - 1 + 0] = crc & 0xFF;

	return len + 3; // total length
}

static size_t ts_write_pmt(const ts_pmt_t *pmt, uint8_t *data)
{
	// 2.4.4.8 Program map table
	// Table 2-33

	uint32_t i = 0;
	uint32_t len = 0;
	uint32_t crc = 0;
	uint8_t *p = NULL;

	data[0] = 0x02;	// program map table

	// skip section_length

	// program_number
	put16(data + 3, pmt->pn);

	// reserved '11'
	// version_number 'xxxxx'
	// current_next_indicator '1'
	data[5] = (uint8_t)(0xC1 | (pmt->ver << 1));

	// section_number/last_section_number
	data[6] = 0x00;
	data[7] = 0x00;

	// reserved '111'
	// PCR_PID 15-bits 0x1FFF
	put16(data + 8, 0xE000 | pmt->PCR_PID);

	// reserved '1111'
	// program_info_lengt 12-bits
	put16(data + 10, 0xF000 | pmt->pminfo_len);
	if(pmt->pminfo_len > 0)
	{
		// fill program info
		assert(pmt->pminfo);
		memcpy(data + 12, pmt->pminfo, pmt->pminfo_len);
	}

	// streams
	p = data + 12 + pmt->pminfo_len;
	for(i = 0; i < pmt->stream_count; i++)
	{
		// stream_type
		*p = (uint8_t)pmt->streams[i].sid;

		// reserved '111'
		// elementary_PID 13-bits
		put16(p + 1, 0xE000 | pmt->streams[i].pid);

		// reserved '1111'
		// ES_info_lengt 12-bits
		put16(p + 3, 0xF000 | pmt->streams[i].esinfo_len);

		// fill elementary stream info
		if(pmt->streams[i].esinfo_len > 0)
		{
			assert(pmt->streams[i].esinfo);
			memcpy(p + 5, pmt->streams[i].esinfo, pmt->streams[i].esinfo_len);
		}

		p += 5 + pmt->streams[i].esinfo_len;
	}

	// section_length
	len = p + 4 - (data + 3); // 4 bytes crc32
	assert(len <= 1021); // shall not exceed 1021 (0x3FD).
	assert(len <= TS_PACKET_SIZE - 7);
	// section_syntax_indicator '1'
	// '0'
	// reserved '11'
	put16(data + 1, 0xb000 | len); 

	// crc32
	crc = crc32(0xffffffff, data, p-data);
	//put32(p, crc);
	p[3] = (crc >> 24) & 0xFF;
	p[2] = (crc >> 16) & 0xFF;
	p[1] = (crc >> 8) & 0xFF;
	p[0] = crc & 0xFF;

	return (p - data) + 4; // total length
}

static void mpeg_ts_write_section_header(const mpeg_ts_enc_context_t *ts, int pid, int cc, const void* payload, size_t len)
{
	uint8_t data[TS_PACKET_SIZE];

	assert(len < TS_PACKET_SIZE - 5); // TS-header + pointer

	// TS Header

	// sync_byte
	data[0] = 0x47;
	// transport_error_indicator = 0
	// payload_unit_start_indicator = 1
	// transport_priority = 0
	data[1] = 0x40 | ((pid >> 8) & 0x1F);
	data[2] = pid & 0xFF;
	// transport_scrambling_control = 0x00
	// adaptation_field_control = 0x11 adaptation and payload
	data[3] = 0x10 | (cc & 0x0F);

//	// Adaptation
//	if(len < TS_PACKET_SIZE - 5)
//	{
//		data[3] |= 0x20; // with adaptation
//		data[4] = TS_PACKET_SIZE - len - 5 - 1; // 4B-Header + 1B-pointer + 1B-self
//		if(data[4] > 0)
//		{
//			// adaptation
//			data[5] = 0; // no flag
//			memset(data+6, 0xFF, data[4]-1);
//		}
//	}

	// pointer
	//data[TS_PACKET_SIZE-len-1] = 0x00;
    data[4] = 0x00;

    // TS Payload
    //memmove(data + TS_PACKET_SIZE - len, payload, len);
    memmove(data + 5, payload, len);
    memset(data+5+len, 0xff, TS_PACKET_SIZE-len-5);

	ts->write(ts->param, data, TS_PACKET_SIZE);
}

static size_t ts_write_pes_header(int64_t pts, int64_t dts, int streamId, uint8_t* data)
{
	uint8_t len = 0;
	uint8_t flags = 0x00;
	uint8_t *p = NULL;

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
	if(PES_VIDEO_STREAM==data[3] && dts /*&& dts != pts*/)
	{
		flags |= 0x40;
		len += 5;
	}
	data[7] = flags;

	// PES_header_data_length : 8
	data[8] = len;

	p = data + 9;
	if(flags & 0x80)
	{
		*p++ = ((flags & 0xFF)>>2) | ((pts >> 28) & 0x0E) | 0x01;
		*p++ = (pts >> 22) & 0xFF;
		*p++ = 0x01 | ((pts >> 14) & 0xFE);
		*p++ = (pts >> 7) & 0xFF;;
		*p++ = 0x01 | ((pts << 1) & 0xFE);
	}

	if(flags & 0x40)
	{
		*p++ = 0x11 | ((dts >> 28) & 0x0E);
		*p++ = (dts >> 22) & 0xFF;
		*p++ = 0x01 | ((dts >> 14) & 0xFE);
		*p++ = (dts >> 7) & 0xFF;;
		*p++ = 0x01 | ((dts << 1) & 0xFE);
	}

	return p - data;
}

#define TS_AF_FLAG_PCR(flag) ((flag) & 0x10)

static int ts_write_pes(mpeg_ts_enc_context_t *tsctx, ts_pes_t *stream, const uint8_t* payload, size_t bytes)
{
	// 2.4.3.6 PES packet
	// Table 2-21

	size_t len = 0;
	int start = 1; // first packet
    int keyframe = 0; // video IDR-frame
	uint8_t *p = NULL;
	uint8_t *pes = NULL;
	uint8_t data[TS_PACKET_SIZE];
	int64_t pcr = 0x8000000000000000L;

	while(bytes > 0)
	{
		stream->cc = (stream->cc + 1 ) % 16;

		// TS Header
		data[0] = 0x47;	// sync_byte
		data[1] = 0x00 | ((stream->pid >>8) & 0x1F);
		data[2] = stream->pid & 0xFF;
		data[3] = 0x10 | (stream->cc & 0x0F); // no adaptation, payload only
		data[4] = 0; // clear adaptation length
		data[5] = 0; // clear adaptation flags

		if(start && stream->pid == tsctx->pat.pmt[0].PCR_PID)
		{
			data[3] |= 0x20; // AF
			data[5] |= 0x10; // PCR_flag
		}

		//if(start && STREAM_VIDEO_H264==stream->sid && h264_idr(payload, bytes))
		//{
		//	//In the PCR_PID the random_access_indicator may only be set to '1' 
		//	//in a transport stream packet containing the PCR fields.
		//	data[3] |= 0x20;
		//	data[5] |= 0x50; // random_access_indicator + PCR_flag
		//}

		if(data[3] & 0x20)
		{
			data[4] = 1; // 1-flag

			if(TS_AF_FLAG_PCR(data[5]))
			{
				data[4] += 6; // 6-PCR
				pcr = (stream->pts - 7*90) * 300; // TODO: delay???
				ts_write_pcr(data + 6, pcr);
			}

			pes = data + 4 + 1 + data[4]; // 4-TS + 1-AF-Len + AF-Payload
		}
		else
		{
			pes = data + 4;
		}

		p = pes;

		// PES header
		if(start)
		{
			data[1] |= 0x40; // payload_unit_start_indicator

			p = pes + ts_write_pes_header(stream->pts, stream->dts, stream->sid, pes);

			if(STREAM_VIDEO_H264 == stream->sid && 0x09 != h264_type(payload, bytes))
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
			if((p - pes - 6) + bytes > 0xFFFF)
				put16(pes + 4, 0); // 2.4.3.7 PES packet => PES_packet_length
			else
				put16(pes + 4, (p - pes - 6) + bytes);
		}

		len = p - data; // TS + PES header length
		if(len + bytes < TS_PACKET_SIZE)
		{
			// move pes header
			if(start)
			{
				memmove(data + (TS_PACKET_SIZE - bytes - (p - pes)), pes, p - pes);
			}

			// adaptation
			if(data[3] & 0x20) // has AF?
			{
				memset(data + 5 + data[4], 0xFF, TS_PACKET_SIZE - (len + bytes));
				data[4] += (uint8_t)(TS_PACKET_SIZE - (len + bytes));
			}
			else
			{
				memset(data + 4, 0xFF, TS_PACKET_SIZE - (len + bytes));
				data[3] |= 0x20;
				data[4] = (uint8_t)(TS_PACKET_SIZE - (len + bytes) - 1);
				data[5] = 0; // no flag				
			}

			len = bytes;
			p = data + 5 + data[4] + (p - pes);
		}
		else
		{
			len = TS_PACKET_SIZE - len;
		}

		// payload
		memcpy(p, payload, len);

		payload += len;
		bytes -= len;
		start = 0;

		// send with TS-header
		tsctx->write(tsctx->param, data, TS_PACKET_SIZE);
	}

	return 0;
}

int mpeg_ts_write(void* ts, int streamId, int64_t pts, int64_t dts, const void* data, size_t bytes)
{
	size_t i, r;
	ts_pes_t *stream = NULL;
	mpeg_ts_enc_context_t *tsctx;
	uint8_t payload[TS_PACKET_SIZE];

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

	// Elementary Stream
	for(i = 0; i < tsctx->pat.pmt[0].stream_count; i++)
	{
		stream = &tsctx->pat.pmt[0].streams[i];
		if(streamId == (int)stream->sid)
		{
			stream->pts = pts;
			stream->dts = dts;
			break;
		}
	}

	ts_write_pes(tsctx, stream, data, bytes);
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
//	mpeg_ts_reset(tsctx);
    tsctx->pat_period = 0;
    tsctx->pcr_period = 0;
    
    tsctx->pat.tsid = 1;
    tsctx->pat.ver = 0;
    tsctx->pat.cc = -1; // +1 => 0
   
    tsctx->pat.pmt_count = 1; // only one program in ts
    tsctx->pat.pmt = tsctx + 1;
    tsctx->pat.pmt[0].pid = 0x100;
    tsctx->pat.pmt[0].pn = 1;
    tsctx->pat.pmt[0].ver = 0x00;
    tsctx->pat.pmt[0].cc = -1; // +1 => 0
    tsctx->pat.pmt[0].pminfo_len = 0;
    tsctx->pat.pmt[0].pminfo = NULL;
    tsctx->pat.pmt[0].PCR_PID = 0x101; // 0x1FFF-don't set PCR

    tsctx->pat.pmt[0].stream_count = 2; // H.264 + AAC
    tsctx->pat.pmt[0].streams = tsctx->pat.pmt + 1;
    tsctx->pat.pmt[0].streams[0].pmt = &tsctx->pat.pmt[0];
    tsctx->pat.pmt[0].streams[0].pid = 0x101;
    tsctx->pat.pmt[0].streams[0].sid = STREAM_VIDEO_H264;
    tsctx->pat.pmt[0].streams[0].esinfo_len = 0x00;
    tsctx->pat.pmt[0].streams[0].esinfo = NULL;
    tsctx->pat.pmt[0].streams[0].cc = -1; // +1 => 0
    tsctx->pat.pmt[0].streams[1].pmt = &tsctx->pat.pmt[0];
    tsctx->pat.pmt[0].streams[1].pid = 0x102;
    tsctx->pat.pmt[0].streams[1].sid = STREAM_AUDIO_AAC;
    tsctx->pat.pmt[0].streams[1].esinfo_len = 0x00;
    tsctx->pat.pmt[0].streams[1].esinfo = NULL;
    tsctx->pat.pmt[0].streams[1].cc = -1; // +1 => 0

	tsctx->write = func;
	tsctx->param = param;
	return tsctx;
}

int mpeg_ts_destroy(void* ts)
{
	uint32_t i, j;
	mpeg_ts_enc_context_t *tsctx = NULL;
	tsctx = (mpeg_ts_enc_context_t*)ts;

	for(i = 0; i < tsctx->pat.pmt_count; i++)
	{
		for(j = 0; j < tsctx->pat.pmt[i].stream_count; j++)
		{
			if(tsctx->pat.pmt[i].streams[j].esinfo)
				free(tsctx->pat.pmt[i].streams[j].esinfo);
		}
	}

	free(tsctx);
	return 0;
}

int mpeg_ts_reset(void* ts)
{
	mpeg_ts_enc_context_t *tsctx;
	tsctx = (mpeg_ts_enc_context_t*)ts;
	tsctx->pat_period = 0;
	tsctx->pcr_period = 0;
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
