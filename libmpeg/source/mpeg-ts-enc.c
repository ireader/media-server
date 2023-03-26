// ITU-T H.222.0(06/2012)
// Information technology - Generic coding of moving pictures and associated audio information: Systems
// 2.4.3.1 Transport stream(p34)

#include "mpeg-ts-internal.h"
#include "mpeg-util.h"
#include "mpeg-ts.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define PCR_DELAY			0 //(700 * 90) // 700ms
#define PAT_PERIOD			(400 * 90) // 500ms
#define PAT_CYCLE			50 // 50fps(audio + video)

#define TS_HEADER_LEN		4 // 1-bytes sync byte + 2-bytes PID + 1-byte CC
#define PES_HEADER_LEN		6 // 3-bytes packet_start_code_prefix + 1-byte stream_id + 2-bytes PES_packet_length

#define TS_PAYLOAD_UNIT_START_INDICATOR 0x40

// adaptation flags
#define AF_FLAG_PCR						0x10
#define AF_FLAG_RANDOM_ACCESS_INDICATOR	0x40 // random_access_indicator

typedef struct _mpeg_ts_enc_context_t
{
    struct pat_t pat;
    int h26x_with_aud;

	int64_t sdt_period;
	int64_t pat_period;
	int64_t pcr_period;
	int64_t pcr_clock; // last pcr time

	int pat_cycle;
	uint16_t pid;

	struct mpeg_ts_func_t func;
	void* param;

	uint8_t payload[1024]; // maximum PAT/PMT payload length
} mpeg_ts_enc_context_t;

static int mpeg_ts_write_section_header(const mpeg_ts_enc_context_t *ts, int pid, unsigned int* cc, const void* payload, size_t len)
{
	int r;
	uint8_t *data = NULL;
	data = ts->func.alloc(ts->param, TS_PACKET_SIZE);
	if(!data) return -ENOMEM;

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

	r = ts->func.write(ts->param, data, TS_PACKET_SIZE);
	ts->func.free(ts->param, data);
	return r;
}

static int ts_write_pes(mpeg_ts_enc_context_t *tsctx, const struct pmt_t* pmt, struct pes_t *stream, const uint8_t* payload, size_t bytes)
{
	// 2.4.3.6 PES packet
	// Table 2-21

	int r = 0;
	size_t len = 0;
	int start = 1; // first packet
    uint8_t *p = NULL;
	uint8_t *data = NULL;
    uint8_t *header = NULL;

	while(0 == r && bytes > 0)
	{
		data = tsctx->func.alloc(tsctx->param, TS_PACKET_SIZE);
		if(!data) return -ENOMEM;

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
		if(start && stream->pid == pmt->PCR_PID)
		{
			data[3] |= 0x20; // +AF
			data[5] |= AF_FLAG_PCR; // +PCR_flag
		}

		// random_access_indicator
		if(start && stream->data_alignment_indicator && PTS_NO_VALUE != stream->pts)
		{
			//In the PCR_PID the random_access_indicator may only be set to '1' 
			//in a transport stream packet containing the PCR fields.
			data[3] |= 0x20; // +AF
			data[5] |= AF_FLAG_RANDOM_ACCESS_INDICATOR; // +random_access_indicator
		}

		if(data[3] & 0x20)
		{
			data[4] = 1; // 1-byte flag

			if(data[5] & AF_FLAG_PCR) // PCR_flag
			{
				int64_t pcr = 0;
				pcr = (PTS_NO_VALUE==stream->dts) ? stream->pts : stream->dts;
				pcr_write(data + 6, (pcr - PCR_DELAY) * 300); // TODO: delay???
				data[4] += 6; // 6-PCR
			}

            header = data + TS_HEADER_LEN + 1 + data[4]; // 4-TS + 1-AF-Len + AF-Payload
		}
		else
		{
            header = data + TS_HEADER_LEN;
		}

		p = header;

		// PES header
		if(start)
		{
			data[1] |= TS_PAYLOAD_UNIT_START_INDICATOR; // payload_unit_start_indicator

            p += pes_write_header(stream, header, TS_PACKET_SIZE - (header - data));

			if(PSI_STREAM_H264 == stream->codecid && !tsctx->h26x_with_aud)
			{
				// 2.14 Carriage of Rec. ITU-T H.264 | ISO/IEC 14496-10 video
				// Each AVC access unit shall contain an access unit delimiter NAL Unit
				nbo_w32(p, 0x00000001);
				p[4] = 0x09; // AUD
				p[5] = 0xF0; // any slice type (0xe) + rbsp stop one bit
				p += 6;
			}
			else if (PSI_STREAM_H265 == stream->codecid && !tsctx->h26x_with_aud)
			{
				// 2.17 Carriage of HEVC
				// Each HEVC access unit shall contain an access unit delimiter NAL unit.
				nbo_w32(p, 0x00000001);
				p[4] = 0x46; // 35-AUD_NUT
				p[5] = 0x01;
				p[6] = 0x50; // B&P&I (0x2) + rbsp stop one bit
				p += 7;
			}
			else if (PSI_STREAM_H266 == stream->codecid && !tsctx->h26x_with_aud)
			{
				// 2.23 Carriage of VVC
				// Each VVC access unit shall contain an access unit delimiter NAL unit
				nbo_w32(p, 0x00000001);
				p[4] = 0x00; // 20-AUD_NUT
				p[5] = 0xA1;
				p[6] = 0x28; // B&P&I (0x2) + rbsp stop one bit
				p += 7;
			}

			// PES_packet_length = PES-Header + Payload-Size
			// A value of 0 indicates that the PES packet length is neither specified nor bounded 
			// and is allowed only in PES packets whose payload consists of bytes from a 
			// video elementary stream contained in transport stream packets
			if((p - header - PES_HEADER_LEN) + bytes > 0xFFFF)
				nbo_w16(header + 4, 0); // 2.4.3.7 PES packet => PES_packet_length
			else
				nbo_w16(header + 4, (uint16_t)((p - header - PES_HEADER_LEN) + bytes));
		}

		len = p - data; // TS + PES header length
		if(len + bytes < TS_PACKET_SIZE)
		{
			// stuffing_len = TS_PACKET_SIZE - (len + bytes)

			// move pes header
			if(p - header > 0)
			{
				assert(start);
				memmove(data + (TS_PACKET_SIZE - bytes - (p - header)), header, p - header);
			}

			// adaptation
			if(data[3] & 0x20) // has AF?
			{
				assert(0 != data[5] && data[4] > 0);
				memset(data + TS_HEADER_LEN + 1 + data[4], 0xFF, TS_PACKET_SIZE - (len + bytes));
				data[4] += (uint8_t)(TS_PACKET_SIZE - (len + bytes));
			}
			else
			{
                assert(len == (size_t)(p - header) + TS_HEADER_LEN);
                data[3] |= 0x20; // +AF
                data[4] = (uint8_t)(TS_PACKET_SIZE - (len + bytes) - 1/*AF length*/);
                if (data[4] > 0) data[5] = 0; // no flag
                if (data[4] > 1) memset(data + 6, 0xFF, TS_PACKET_SIZE - (len + bytes) - 2);
			}
            len = bytes;

			p = data + 5 + data[4] + (p - header);
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
		r = tsctx->func.write(tsctx->param, data, TS_PACKET_SIZE);
		tsctx->func.free(tsctx->param, data);
	}

	return r;
}

static struct pes_t *mpeg_ts_find(mpeg_ts_enc_context_t *ts, int pid, struct pmt_t** pmt)
{
    size_t i, j;
    struct pes_t* stream;

    for (i = 0; i < ts->pat.pmt_count; i++)
    {
        *pmt = &ts->pat.pmts[i];
        for (j = 0; j < (*pmt)->stream_count; j++)
        {
            stream = &(*pmt)->streams[j];
            if (pid == (int)stream->pid)
                return stream;
        }
    }

    return NULL;
}

int mpeg_ts_write(void* ts, int pid, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes)
{
	int r = 0;
	size_t i, n;
    struct pmt_t *pmt = NULL;
	struct pes_t *stream = NULL;
	mpeg_ts_enc_context_t *tsctx;

	tsctx = (mpeg_ts_enc_context_t*)ts;
    stream = mpeg_ts_find(tsctx, pid, &pmt);
    if (NULL == stream)
        return -ENOENT; // not found

    stream->pts = pts;
    stream->dts = dts;
    stream->data_alignment_indicator = (flags & MPEG_FLAG_IDR_FRAME) ? 1 : 0; // idr frame
    tsctx->h26x_with_aud = (flags & MPEG_FLAG_H264_H265_WITH_AUD) ? 1 : 0;
    // set PCR_PID
    //assert(1 == tsctx->pat.pmt_count);
    if (0x1FFF == pmt->PCR_PID || (PES_SID_VIDEO == (stream->sid & PES_SID_VIDEO) && pmt->PCR_PID != stream->pid))
    {
        pmt->PCR_PID = stream->pid;
        tsctx->pat_period = 0;
		tsctx->pat_cycle = 0;
    }

	if (pmt->PCR_PID == stream->pid)
		++tsctx->pcr_clock;

	// Add PAT and PMT for video IDR frame
	if(0 == ++tsctx->pat_cycle % PAT_CYCLE || 0 == tsctx->pat_period || tsctx->pat_period + PAT_PERIOD <= dts || (PES_SID_VIDEO == (stream->sid & PES_SID_VIDEO) && (flags & MPEG_FLAG_IDR_FRAME)))
	{
		tsctx->pat_cycle = 0;
		tsctx->pat_period = dts;

		if (0 == tsctx->sdt_period)
		{
			// SDT
			tsctx->sdt_period = dts;
			n = sdt_write(&tsctx->pat, tsctx->payload);
			r = mpeg_ts_write_section_header(ts, TS_PID_SDT, &tsctx->pat.cc /*fixme*/ , tsctx->payload, n);
			if (0 != r) return r;
		}

		// PAT(program_association_section)
		n = pat_write(&tsctx->pat, tsctx->payload);
		r = mpeg_ts_write_section_header(ts, TS_PID_PAT, &tsctx->pat.cc, tsctx->payload, n); // PID = 0x00 program association table
		if (0 != r) return r;

		// PMT(Transport stream program map section)
		for(i = 0; i < tsctx->pat.pmt_count; i++)
		{
			n = pmt_write(&tsctx->pat.pmts[i], tsctx->payload);
			r = mpeg_ts_write_section_header(ts, tsctx->pat.pmts[i].pid, &tsctx->pat.pmts[i].cc, tsctx->payload, n);
			if (0 != r) return r;
		}
	}

	return ts_write_pes(tsctx, pmt, stream, data, bytes);
}

void* mpeg_ts_create(const struct mpeg_ts_func_t *func, void* param)
{
	mpeg_ts_enc_context_t *tsctx = NULL;

	assert(func);
	tsctx = (mpeg_ts_enc_context_t *)calloc(1, sizeof(mpeg_ts_enc_context_t));
	if(!tsctx)
		return NULL;

	mpeg_ts_reset(tsctx);

    tsctx->pat.tsid = 1;
    tsctx->pat.ver = 0x00;
	tsctx->pat.cc = 0;
	tsctx->pid = 0x100;

	//tsctx->pat.pmt_count = 1; // only one program in ts
    //tsctx->pat.pmts[0].pid = 0x100;
    //tsctx->pat.pmts[0].pn = 1;
    //tsctx->pat.pmts[0].ver = 0x00;
    //tsctx->pat.pmts[0].cc = 0;
    //tsctx->pat.pmts[0].pminfo_len = 0;
    //tsctx->pat.pmts[0].pminfo = NULL;
    //tsctx->pat.pmts[0].PCR_PID = 0x1FFF; // 0x1FFF-don't set PCR

	//tsctx->pat.pmts[0].stream_count = 2; // H.264 + AAC
	//tsctx->pat.pmts[0].streams[0].pid = 0x101;
	//tsctx->pat.pmts[0].streams[0].sid = PES_SID_AUDIO;
	//tsctx->pat.pmts[0].streams[0].codecid = PSI_STREAM_AAC;
	//tsctx->pat.pmts[0].streams[1].pid = 0x102;
	//tsctx->pat.pmts[0].streams[1].sid = PES_SID_VIDEO;
	//tsctx->pat.pmts[0].streams[1].codecid = PSI_STREAM_H264;

	memcpy(&tsctx->func, func, sizeof(tsctx->func));
	tsctx->param = param;
	return tsctx;
}

int mpeg_ts_destroy(void* ts)
{
	mpeg_ts_enc_context_t *tsctx;
	tsctx = (mpeg_ts_enc_context_t*)ts;

	pat_clear(&tsctx->pat);
	free(tsctx);
	return 0;
}

int mpeg_ts_reset(void* ts)
{
	mpeg_ts_enc_context_t *tsctx;
	tsctx = (mpeg_ts_enc_context_t*)ts;
//	tsctx->sdt_period = 0;
	tsctx->pat_period = 0;
	tsctx->pcr_period = 80 * 90; // 100ms maximum
	tsctx->pcr_clock = 0;
	tsctx->pat_cycle = 0;
	return 0;
}

int mpeg_ts_add_program(void* ts, uint16_t pn, const void* info, int bytes)
{
	unsigned int i;
	struct pmt_t* pmt;
	mpeg_ts_enc_context_t* tsctx;

	if (pn < 1 || bytes < 0 || bytes >= (1 << 12))
		return -1; // EINVAL: pminfo-len 12-bits

	tsctx = (mpeg_ts_enc_context_t*)ts;
	for (i = 0; i < tsctx->pat.pmt_count; i++)
	{
		pmt = &tsctx->pat.pmts[i];
		if (pmt->pn == pn)
			return -1; // EEXIST
	}

	assert(tsctx->pat.pmt_count == i);
	pmt = pat_alloc_pmt(&tsctx->pat);
	if (!pmt)
		return -1; // E2BIG

	pmt->pid = tsctx->pid++;
	pmt->pn = pn;
	pmt->ver = 0x00;
	pmt->cc = 0;
	pmt->PCR_PID = 0x1FFF; // 0x1FFF-don't set PCR

	if (bytes > 0 && info)
	{
		pmt->pminfo = (uint8_t*)malloc(bytes);
		if (!pmt->pminfo)
			return -1; // ENOMEM
		memcpy(pmt->pminfo, info, bytes);
		pmt->pminfo_len = bytes;
	}

	tsctx->pat.pmt_count++;
	mpeg_ts_reset(ts); // update PAT/PMT
	return 0;
}

int mpeg_ts_remove_program(void* ts, uint16_t pn)
{
	unsigned int i;
	struct pmt_t* pmt = NULL;
	mpeg_ts_enc_context_t* tsctx;

	tsctx = (mpeg_ts_enc_context_t*)ts;
	for (i = 0; i < tsctx->pat.pmt_count; i++)
	{
		pmt = &tsctx->pat.pmts[i];
		if (pmt->pn != pn)
			continue;
		
		pmt_clear(pmt);
		if (i + 1 < tsctx->pat.pmt_count)
			memmove(&tsctx->pat.pmts[i], &tsctx->pat.pmts[i + 1], (tsctx->pat.pmt_count - i - 1) * sizeof(tsctx->pat.pmts[0]));
		tsctx->pat.pmt_count--;
		mpeg_ts_reset(ts); // update PAT/PMT
		return 0;
	}

	return -1; // ENOTFOUND
}

static int mpeg_ts_pmt_add_stream(mpeg_ts_enc_context_t* ts, struct pmt_t* pmt, int codecid, const void* extra_data, size_t extra_data_size)
{
	struct pes_t* stream = NULL;
	if (!ts || !pmt || pmt->stream_count >= sizeof(pmt->streams) / sizeof(pmt->streams[0]))
	{
		assert(0);
		return -1;
	}

	stream = &pmt->streams[pmt->stream_count];
	stream->codecid = (uint8_t)codecid;
	stream->pid = (uint16_t)ts->pid++;
	stream->esinfo_len = 0;
	stream->esinfo = NULL;

	// stream id
	// Table 2-22 - Stream_id assignments
	if (mpeg_stream_type_video(codecid))
	{
		// Rec. ITU-T H.262 | ISO/IEC 13818-2, ISO/IEC 11172-2, ISO/IEC 14496-2 
		// or Rec. ITU-T H.264 | ISO/IEC 14496-10 video stream number
		stream->sid = PES_SID_VIDEO;
	}
	else if (mpeg_stream_type_audio(codecid))
	{
		// ISO/IEC 13818-3 or ISO/IEC 11172-3 or ISO/IEC 13818-7 or ISO/IEC 14496-3
		// audio stream number
		stream->sid = PES_SID_AUDIO;
	}
	else
	{
		// private_stream_1
		stream->sid = PES_SID_PRIVATE_1;
	}

	if (extra_data_size > 0 && extra_data)
	{
		stream->esinfo = malloc(extra_data_size);
		if (!stream->esinfo)
			return -ENOMEM;
		memcpy(stream->esinfo, extra_data, extra_data_size);
		stream->esinfo_len = (uint16_t)extra_data_size;
	}

	pmt->stream_count++;
	pmt->ver = (pmt->ver + 1) % 32;
	mpeg_ts_reset(ts); // immediate update pat/pmt
	return stream->pid;
}

int mpeg_ts_add_stream(void* ts, int codecid, const void* extra_data, size_t extra_data_size)
{
    struct pmt_t *pmt = NULL;
    mpeg_ts_enc_context_t *tsctx;

    tsctx = (mpeg_ts_enc_context_t*)ts;
	if (0 == tsctx->pat.pmt_count)
	{
		// add default program
		if (0 != mpeg_ts_add_program(tsctx, 1, NULL, 0))
			return -1;
	}
    pmt = &tsctx->pat.pmts[0];

	return mpeg_ts_pmt_add_stream(tsctx, pmt, codecid, extra_data, extra_data_size);
}

int mpeg_ts_add_program_stream(void* ts, uint16_t pn, int codecid, const void* extra_data, size_t extra_data_size)
{
	unsigned int i;
	struct pmt_t* pmt = NULL;
	mpeg_ts_enc_context_t* tsctx;

	tsctx = (mpeg_ts_enc_context_t*)ts;
	for (i = 0; i < tsctx->pat.pmt_count; i++)
	{
		pmt = &tsctx->pat.pmts[i];
		if (pmt->pn == pn)
			return mpeg_ts_pmt_add_stream(tsctx, pmt, codecid, extra_data, extra_data_size);
	}

	return -1; // ENOTFOUND: program not found
}
