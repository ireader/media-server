// ITU-T H.222.0(06/2012)
// Information technology ¨C Generic coding of moving pictures and associated audio information: Systems
// 2.4.3.1 Transport stream(p34)

#include "mpeg-ts-proto.h"
#include "mpeg-util.h"
#include "mpeg-ts.h"
#include <errno.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <assert.h>

#define PCR_DELAY			0 //(700 * 90) // 700ms
#define N_MPEG_TS_STREAM	8

#define TS_HEADER_LEN		4 // 1-bytes sync byte + 2-bytes PID + 1-byte CC
#define PES_HEADER_LEN		6 // 3-bytes packet_start_code_prefix + 1-byte stream_id + 2-bytes PES_packet_length

#define TS_PAYLOAD_UNIT_START_INDICATOR 0x40

// adaptation flags
#define AF_FLAG_PCR						0x10
#define AF_FLAG_RANDOM_ACCESS_INDICATOR	0x40 // random_access_indicator

typedef struct _mpeg_ts_enc_context_t
{
	pat_t pat;

	unsigned int pat_period;
	int64_t pcr_period;
	int64_t pcr_clock; // last pcr time

	struct mpeg_ts_func_t func;
	void* param;

	uint8_t payload[1024]; // maximum PAT/PMT payload length
} mpeg_ts_enc_context_t;

static int mpeg_ts_write_section_header(const mpeg_ts_enc_context_t *ts, int pid, unsigned int* cc, const void* payload, size_t len)
{
	uint8_t *data = NULL;
	data = ts->func.alloc(ts->param, TS_PACKET_SIZE);
	if(!data) return ENOMEM;

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
	// adaptation_field_control = 0x01-No adaptation_field, payload only, 0x03-adaptation and payload
	data[3] = 0x10 | (*cc & 0x0F);
	*cc = (*cc + 1) % 16; // update continuity_counter

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

	// pointer (payload_unit_start_indicator==1)
	//data[TS_PACKET_SIZE-len-1] = 0x00;
    data[4] = 0x00;

    // TS Payload
    //memmove(data + TS_PACKET_SIZE - len, payload, len);
    memmove(data + 5, payload, len);
    memset(data+5+len, 0xff, TS_PACKET_SIZE-len-5);

	ts->func.write(ts->param, data, TS_PACKET_SIZE);
	ts->func.free(ts->param, data);
	return 0;
}

static int ts_write_pes(mpeg_ts_enc_context_t *tsctx, pes_t *stream, const uint8_t* payload, size_t bytes)
{
	// 2.4.3.6 PES packet
	// Table 2-21

	size_t len = 0;
	int start = 1; // first packet
    int keyframe; // video IDR-frame
	uint8_t *p = NULL;
	uint8_t *pes = NULL;
	uint8_t *data = NULL;

	keyframe = (PSI_STREAM_H264 == stream->avtype && find_h264_keyframe(payload, bytes));

	while(bytes > 0)
	{
		data = tsctx->func.alloc(tsctx->param, TS_PACKET_SIZE);
		if(!data) return ENOMEM;

		// TS Header
		data[0] = 0x47;	// sync_byte
		data[1] = 0x00 | ((stream->pid >>8) & 0x1F);
		data[2] = stream->pid & 0xFF;
		data[3] = 0x10 | (stream->cc & 0x0F); // no adaptation, payload only
		data[4] = 0; // clear adaptation length
		data[5] = 0; // clear adaptation flags

		stream->cc = (stream->cc + 1) % 16;

		// 2.7.2 Frequency of coding the program clock reference
		// http://www.bretl.com/mpeghtml/SCR.HTM
		// the maximum between PCRs is 100ms.  
		if(start && stream->pid == tsctx->pat.pmt[0].PCR_PID)
		{
			data[3] |= 0x20; // +AF
			data[5] |= AF_FLAG_PCR; // +PCR_flag
		}

		// random_access_indicator
		if(start && keyframe && PTS_NO_VALUE != stream->pts)
		{
			//In the PCR_PID the random_access_indicator may only be set to '1' 
			//in a transport stream packet containing the PCR fields.
			data[3] |= 0x20; // +AF
			data[5] |= AF_FLAG_RANDOM_ACCESS_INDICATOR; // +random_access_indicator
		}

		if(data[3] & 0x20)
		{
			data[4] = 1; // 1-byte flag length

			if(data[5] & AF_FLAG_PCR) // PCR_flag
			{
				int64_t pcr = 0;
				pcr = (PTS_NO_VALUE==stream->dts) ? stream->pts : stream->dts;
				pcr_write(data + 6, (pcr - PCR_DELAY) * 300); // TODO: delay???
				data[4] += 6; // 6-PCR
			}

			pes = data + TS_HEADER_LEN + 1 + data[4]; // 4-TS + 1-AF-Len + AF-Payload
		}
		else
		{
			pes = data + TS_HEADER_LEN;
		}

		p = pes;

		// PES header
		if(start)
		{
			data[1] |= TS_PAYLOAD_UNIT_START_INDICATOR; // payload_unit_start_indicator

			p = pes + pes_write_header(stream->pts, stream->dts, stream->sid, pes);

			if(PSI_STREAM_H264 == stream->avtype && 0 == find_h264_access_unit_delimiter(payload, bytes))
			{
				// 2.14 Carriage of Rec. ITU-T H.264 | ISO/IEC 14496-10 video
				// Each AVC access unit shall contain an access unit delimiter NAL Unit
				nbo_w32(p, 0x00000001);
				p[4] = 0x09; // AUD
				p[5] = 0xF0; // any slice type (0xe) + rbsp stop one bit
				p += 6;
			}

			// PES_packet_length = PES-Header + Payload-Size
			// A value of 0 indicates that the PES packet length is neither specified nor bounded 
			// and is allowed only in PES packets whose payload consists of bytes from a 
			// video elementary stream contained in transport stream packets
			if((p - pes - PES_HEADER_LEN) + bytes > 0xFFFF)
				nbo_w16(pes + 4, 0); // 2.4.3.7 PES packet => PES_packet_length
			else
				nbo_w16(pes + 4, (uint16_t)((p - pes - PES_HEADER_LEN) + bytes));
		}

		len = p - data; // TS + PES header length
		if(len + bytes < TS_PACKET_SIZE)
		{
			// stuffing_len = TS_PACKET_SIZE - (len + bytes)

			// move pes header
			if(p - pes > 0)
			{
				assert(start);
				memmove(data + (TS_PACKET_SIZE - bytes - (p - pes)), pes, p - pes);
			}

			// adaptation
			if(data[3] & 0x20) // has AF?
			{
				assert(0 != data[5]);
				memset(data + 5 + data[4], 0xFF, TS_PACKET_SIZE - (len + bytes));
				data[4] += (uint8_t)(TS_PACKET_SIZE - (len + bytes));
			}
			else
			{
				memset(data + 4, 0xFF, TS_PACKET_SIZE - (len + bytes));
				data[3] |= 0x20;
				data[4] = (uint8_t)(TS_PACKET_SIZE - (len + bytes) - 1);
				if(data[4] > 0)
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
		tsctx->func.write(tsctx->param, data, TS_PACKET_SIZE);
		tsctx->func.free(tsctx->param, data);
	}

	return 0;
}

int mpeg_ts_write(void* ts, int avtype, int64_t pts, int64_t dts, const void* data, size_t bytes)
{
	size_t i, r;
	pes_t *stream = NULL;
	mpeg_ts_enc_context_t *tsctx;

	tsctx = (mpeg_ts_enc_context_t*)ts;

	// Elementary Stream
	for (i = 0; i < tsctx->pat.pmt[0].stream_count; i++)
	{
		stream = &tsctx->pat.pmt[0].streams[i];

		// enable audio stream type change (AAC -> MP3)
		if (PES_SID_AUDIO == stream->sid 
			&& (PSI_STREAM_AAC == avtype || PSI_STREAM_MP3 == avtype)
			&& avtype != stream->avtype)
		{
			stream->avtype = (uint8_t)avtype;
			mpeg_ts_reset(tsctx);
		}

		if (avtype == (int)stream->avtype)
		{
			stream->pts = pts;
			stream->dts = dts;

			if (0x1FFF == tsctx->pat.pmt[0].PCR_PID 
				|| (0xE0 == (tsctx->pat.pmt[0].streams[i].sid&PES_SID_VIDEO) && tsctx->pat.pmt[0].PCR_PID != tsctx->pat.pmt[0].streams[i].pid))
			{
				tsctx->pat.pmt[0].PCR_PID = tsctx->pat.pmt[0].streams[i].pid;
				tsctx->pat_period = 0;
			}

			if (tsctx->pat.pmt[0].PCR_PID == tsctx->pat.pmt[0].streams[i].pid)
				++tsctx->pcr_clock;
			break;
		}
	}

	if(0 == tsctx->pat_period)
	{
		// PAT(program_association_section)
		r = pat_write(&tsctx->pat, tsctx->payload);
		mpeg_ts_write_section_header(ts, 0x00, &tsctx->pat.cc, tsctx->payload, r); // PID = 0x00 program association table

		// PMT(Transport stream program map section)
		for(i = 0; i < tsctx->pat.pmt_count; i++)
		{
			r = pmt_write(&tsctx->pat.pmt[i], tsctx->payload);
			mpeg_ts_write_section_header(ts, tsctx->pat.pmt[i].pid, &tsctx->pat.pmt[i].cc, tsctx->payload, r);
		}
	}

	tsctx->pat_period = (tsctx->pat_period + 1) % 200;

	ts_write_pes(tsctx, stream, data, bytes);
	return 0;
}

void* mpeg_ts_create(const struct mpeg_ts_func_t *func, void* param)
{
	mpeg_ts_enc_context_t *tsctx = NULL;

	assert(func);
	tsctx = (mpeg_ts_enc_context_t *)calloc(1, sizeof(mpeg_ts_enc_context_t) 
											+ sizeof(tsctx->pat.pmt[0])
											+ 2 * sizeof(tsctx->pat.pmt[0].streams[0]));
	if(!tsctx)
		return NULL;

	mpeg_ts_reset(tsctx);

    tsctx->pat.tsid = 1;
    tsctx->pat.ver = 0x00;
	tsctx->pat.cc = 0;

    tsctx->pat.pmt_count = 1; // only one program in ts
    tsctx->pat.pmt = (pmt_t*)(tsctx + 1);
    tsctx->pat.pmt[0].pid = 0x100;
    tsctx->pat.pmt[0].pn = 1;
    tsctx->pat.pmt[0].ver = 0x00;
    tsctx->pat.pmt[0].cc = 0;
    tsctx->pat.pmt[0].pminfo_len = 0;
    tsctx->pat.pmt[0].pminfo = NULL;
    tsctx->pat.pmt[0].PCR_PID = 0x1FFF; // 0x1FFF-don't set PCR

    tsctx->pat.pmt[0].stream_count = 2; // H.264 + AAC
    tsctx->pat.pmt[0].streams = (pes_t*)(tsctx->pat.pmt + 1);
	tsctx->pat.pmt[0].streams[0].pmt = &tsctx->pat.pmt[0];
	tsctx->pat.pmt[0].streams[0].pid = 0x101;
	tsctx->pat.pmt[0].streams[0].sid = PES_SID_AUDIO;
	tsctx->pat.pmt[0].streams[0].avtype = PSI_STREAM_AAC;
    tsctx->pat.pmt[0].streams[1].pmt = &tsctx->pat.pmt[0];
    tsctx->pat.pmt[0].streams[1].pid = 0x102;
    tsctx->pat.pmt[0].streams[1].sid = PES_SID_VIDEO;
	tsctx->pat.pmt[0].streams[1].avtype = PSI_STREAM_H264;

	memcpy(&tsctx->func, func, sizeof(tsctx->func));
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
	tsctx->pcr_period = 80 * 90; // 100ms maximum
	tsctx->pcr_clock = 0;
	return 0;
}

int mpeg_ts_add_stream(void* ts, int avtype)
{
	pmt_t *pmt = NULL;
	mpeg_ts_enc_context_t *tsctx;

	tsctx = (mpeg_ts_enc_context_t*)ts;
	pmt = tsctx->pat.pmt;
	if(pmt->stream_count + 1 >= N_MPEG_TS_STREAM)
	{
		assert(0);
		return -1;
	}

	pmt = &tsctx->pat.pmt[0];
	pmt->streams[pmt->stream_count].avtype = (uint8_t)avtype;
	pmt->streams[pmt->stream_count].pid = (uint16_t)(TS_PID_USER + pmt->stream_count);
	pmt->streams[pmt->stream_count].esinfo_len = 0;
	pmt->streams[pmt->stream_count].esinfo = NULL;

	// stream id
	// Table 2-22 ¨C Stream_id assignments
	if(PSI_STREAM_H264==avtype || PSI_STREAM_MPEG4==avtype || PSI_STREAM_MPEG2==avtype || PSI_STREAM_MPEG1==avtype || PSI_STREAM_VIDEO_VC1==avtype || PSI_STREAM_VIDEO_SVAC==avtype)
	{
		// Rec. ITU-T H.262 | ISO/IEC 13818-2, ISO/IEC 11172-2, ISO/IEC 14496-2 
		// or Rec. ITU-T H.264 | ISO/IEC 14496-10 video stream number
		pmt->streams[pmt->stream_count].sid = PES_SID_VIDEO;
	}
	else if(PSI_STREAM_AAC==avtype || PSI_STREAM_MPEG4_AAC_LATM==avtype || PSI_STREAM_MPEG4_AAC==avtype || PSI_STREAM_MP3==avtype || PSI_STREAM_AUDIO_AC3==avtype 
		|| PSI_STREAM_AUDIO_SVAC==avtype || PSI_STREAM_AUDIO_G711==avtype || PSI_STREAM_AUDIO_G722==avtype || PSI_STREAM_AUDIO_G723==avtype || PSI_STREAM_AUDIO_G729==avtype)
	{
		// ISO/IEC 13818-3 or ISO/IEC 11172-3 or ISO/IEC 13818-7 or ISO/IEC 14496-3
		// audio stream number
		pmt->streams[pmt->stream_count].sid = PES_SID_AUDIO;
	}
	else
	{
		// private_stream_1
		pmt->streams[pmt->stream_count].sid = PES_SID_PRIVATE_1;
	}

	++pmt->stream_count;
	pmt->ver = (pmt->ver+1) % 32;

	mpeg_ts_reset(ts); // immediate update pat/pmt
	return 0;
}
