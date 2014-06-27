// ITU-T H.222.0(06/2012)
// Information technology ¨C Generic coding of moving pictures and associated audio information: Systems
//
#include <stdlib.h>
#include "mpeg-ts-proto.h"
#include "mpeg-ts.h"
#include "crc32.h"
#include "dlog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

typedef struct _mpeg_ts_dec_context_t
{
	ts_pat_t pat;
	ts_pmt_t pmt[1];
	ts_pes_t pes[2];
} mpeg_ts_dec_context_t;

static mpeg_ts_dec_context_t tsctx;

int ts_pat_dec(const uint8_t* data, int bytes, ts_pat_t *pat)
{
	// 2.4.4.3 Program association table
	// Table 2-30

	int i = 0;
	int j = 0;
	uint32_t crc = 0;

	uint32_t table_id = data[0];
	uint32_t section_syntax_indicator = (data[1] >> 7) & 0x01;
	uint32_t zero = (data[1] >> 6) & 0x01;
	uint32_t reserved = (data[1] >> 4) & 0x03;
	uint32_t section_length = ((data[1] & 0x0F) << 8) | data[2];
	uint32_t transport_stream_id = (data[3] << 8) | data[4];
	uint32_t reserved2 = (data[5] >> 6) & 0x03;
	uint32_t version_number = (data[5] >> 1) & 0x1F;
	uint32_t current_next_indicator = data[5] & 0x01;
	uint32_t sector_number = data[6];
	uint32_t last_sector_number = data[7];

	dlog_log("PAT: %0x %0x %0x %0x %0x %0x %0x %0x\n", (unsigned int)data[0], (unsigned int)data[1], (unsigned int)data[2], (unsigned int)data[3], (unsigned int)data[4], (unsigned int)data[5], (unsigned int)data[6], (unsigned int)data[7]);

	assert(0x00 == table_id);
	assert(1 == section_syntax_indicator);
	pat->tsid = transport_stream_id;
	pat->ver = version_number;

	for(i = 8; i < section_length + 8 - 4 - 5; i += 4)
	{
		pat->pmt[j].pn = (data[i] << 8) | data[i+1];
		pat->pmt[j].pid = ((data[i+2] & 0x1F) << 8) | data[i+3];
		dlog_log("PAT[%d]: pn: %0x, pid: %0x\n", j, pat->pmt[j].pn, pat->pmt[j].pid);
		j++;
	}

	pat->pmt_count = j;

	//assert(i+4 == bytes);
	//crc = (data[i] << 24) | (data[i+1] << 16) | (data[i+2] << 8) | data[i+3];
	//crc = crc32(-1, data, bytes-4);
	assert(0 == crc32(0xffffffff, data, section_length+3));
	return 0;
}

int ts_pmt_dec(const uint8_t* data, int bytes, ts_pmt_t *pmt)
{
	int i = 0;
	int j = 0;
	int k = 0;
	int n = 0;

	uint32_t table_id = data[0];
	uint32_t section_syntax_indicator = (data[1] >> 7) & 0x01;
	uint32_t zero = (data[1] >> 6) & 0x01;
	uint32_t reserved = (data[1] >> 4) & 0x03;
	uint32_t section_length = ((data[1] & 0x0F) << 8) | data[2];
	uint32_t program_number = (data[3] << 8) | data[4];
	uint32_t reserved2 = (data[5] >> 6) & 0x03;
	uint32_t version_number = (data[5] >> 1) & 0x1F;
	uint32_t current_next_indicator = data[5] & 0x01;
	uint32_t sector_number = data[6];
	uint32_t last_sector_number = data[7];
	uint32_t reserved3 = (data[8] >> 6) & 0x03;
	uint32_t PCR_PID = ((data[8] & 0x1F) << 8) | data[9];
	uint32_t reserved4 = (data[10] >> 4) & 0x0F;
	uint32_t program_info_length = ((data[10] & 0x0F) << 8) | data[11];

	dlog_log("PMT: %0x %0x %0x %0x %0x %0x %0x %0x, %0x, %0x, %0x, %0x\n", (unsigned int)data[0], (unsigned int)data[1], (unsigned int)data[2], (unsigned int)data[3], (unsigned int)data[4], (unsigned int)data[5], (unsigned int)data[6],(unsigned int)data[7],(unsigned int)data[8],(unsigned int)data[9],(unsigned int)data[10],(unsigned int)data[11]);

	assert(0x02 == table_id);
	assert(1 == section_syntax_indicator);
	assert(0 == sector_number);
	assert(0 == last_sector_number);
	pmt->PCR_PID = PCR_PID;
	pmt->pn = program_number;
	pmt->ver = version_number;

	i = 12;
	if(program_info_length)
	{
		// descriptor()
	}

	for(j = 0; j < section_length - 9 - pmt->pminfo_len - 4; j += 5)
	{
		pmt->streams[n].sid = data[i+j];
		pmt->streams[n].pid = ((data[i+j+1] & 0x1F) << 8) | data[i+j+2];
		pmt->streams[n].esinfo_len = ((data[i+j+3] & 0x0F) << 8) | data[i+j+4];
		dlog_log("PMT[%d]: sid: %0x, pid: %0x, eslen: %d\n", j, pmt->streams[n].sid, pmt->streams[n].pid, pmt->streams[n].esinfo_len);

		for(k = 0; k < pmt->streams[n].esinfo_len; k++)
		{
			// descriptor
		}

		j += pmt->streams[n].esinfo_len;
		n++;
	}

	pmt->stream_count = n;

	i += j;
	//assert(i+4 == bytes);
	//crc = (data[i] << 24) | (data[i+1] << 16) | (data[i+2] << 8) | data[i+3];
	assert(0 == crc32(-1, data, section_length+3));
	return 0;
}

static int pes_payload(void* param, const uint8_t* data, int bytes)
{
    ts_pes_t *pes;
    pes = (ts_pes_t*)param;

    memcpy(pes->payload + pes->payload_len, data, bytes);
    pes->payload_len += bytes;
	return 0;
}

int pes_dec(void* param, const uint8_t* data, int bytes, ts_pes_t *pes)
{
	int i = 0;
	int n = 0;

	uint32_t packet_start_code_prefix = (data[0] << 16) | (data[1] << 8) | data[2];
	uint32_t stream_id = data[3];
	uint32_t PES_packet_length = (data[4] << 8) | data[5];
	dlog_log("PES: %0x %0x %0x %0x %0x %0x\n", (unsigned int)data[0], (unsigned int)data[1], (unsigned int)data[2], (unsigned int)data[3], (unsigned int)data[4], (unsigned int)data[5]);

	assert(0x00000001 == packet_start_code_prefix);
	pes->len = PES_packet_length;
    assert(0xe0 == stream_id || 0xc0 == stream_id);

	switch(stream_id)
	{
	case PES_PROGRAM_STREAM_MAP:
	case PES_PRIVATE_STREAM_2:
	case PES_ECM:
	case PES_EMM:
	case PES_PROGRAM_STREAM_DIRECTORY:
	case PES_DSMCC_STREAM:
	case PES_H222_E_STREAM:
		// stream data
		break;

	case PES_PADDING_STREAM:
		// padding
		break;

	default:
		i = 6;
		assert(0x02 == ((data[i] >> 6) & 0x3));
		pes->PES_scrambling_control = (data[i] >> 4) & 0x3;
		pes->PES_priority = (data[i] >> 3) & 0x1;
		pes->data_alignment_indicator = (data[i] >> 2) & 0x1;
		pes->copyright = (data[i] >> 1) & 0x1;
		pes->original_or_copy = data[i] & 0x1;

		i++;
		pes->PTS_DTS_flags = (data[i] >> 6) & 0x3;
		pes->ESCR_flag = (data[i] >> 5) & 0x1;
		pes->ES_rate_flag = (data[i] >> 4) & 0x1;
		pes->DSM_trick_mode_flag = (data[i] >> 3) & 0x1;
		pes->additional_copy_info_flag = (data[i] >> 2) & 0x1;
		pes->PES_CRC_flag = (data[i] >> 1) & 0x1;
		pes->PES_extension_flag = data[i] & 0x1;

		i++;
		pes->PES_header_data_length = data[i];

		dlog_log("PES: flag: %0x/%0x len: %d\n", (unsigned int)data[6], (unsigned int)data[7], (unsigned int)data[8]);

		i++;
		if(0x02 == pes->PTS_DTS_flags)
		{
			assert(0x20 == (data[i] & 0xF0));
			pes->pts = (((int64_t)(data[i] >> 1) & 0x07) << 30) | (data[i+1] << 22) | (((data[i+2] >> 1) & 0x7F) << 15) | (data[i+3] << 7) | ((data[i+4] >> 1) & 0x7F);

			i += 5;
		}
		else if(0x03 == pes->PTS_DTS_flags)
		{
			assert(0x30 == (data[i] & 0xF0));
			pes->pts = (((int64_t)(data[i] >> 1) & 0x07) << 30) | (data[i+1] << 22) | (((data[i+2] >> 1) & 0x7F) << 15) | (data[i+3] << 7) | ((data[i+4] >> 1) & 0x7F);
			i += 5;

			assert(0x10 == (data[i] & 0xF0));
			pes->dts = (((int64_t)(data[i] >> 1) & 0x07) << 30) | (data[i+1] << 22) | (((data[i+2] >> 1) & 0x7F) << 15) | (data[i+3] << 7) | ((data[i+4] >> 1) & 0x7F);
			i += 5;
		}

		if(pes->ESCR_flag)
		{
			pes->ESCR_base = (((int64_t)(data[i] >> 3) & 0x07) << 30) | ((data[i] & 0x03) << 28) | (data[i+1] << 20) | (((data[i+2] >> 3) & 0x1F) << 15) | ((data[i+2] & 0x3) << 13) | (data[i+3] << 5) | ((data[i+4] >> 3) & 0x1F);
			pes->ESCR_extension = ((data[i+4] & 0x03) << 7) | ((data[i+5] >> 1) & 0x3F);
			i += 6;
		}

		if(pes->ES_rate_flag)
		{
			pes->ES_rate = ((data[i] & 0x7F) << 15) | (data[i+1] << 7) | ((data[i+2] >> 1) & 0x7F);
			i += 3;
		}

		if(pes->DSM_trick_mode_flag)
		{
			// TODO:
			i += 1;
		}

		if(pes->additional_copy_info_flag)
		{
			i += 1;
		}

		if(pes->PES_CRC_flag)
		{
			i += 2;
		}

		if(pes->PES_extension_flag)
		{
		}

		// payload
		i = 6 + 3 + pes->PES_header_data_length;
		pes_payload(pes, data + i, bytes - i);
		pes->len -= bytes - 6;
	}

	return 0;
}

static uint32_t ts_packet_adaptation(const uint8_t* data, int bytes, ts_adaptation_field_t *adp)
{
	// 2.4.3.4 Adaptation field
	// Table 2-6
	uint32_t i = 0;
	uint32_t j = 0;

	adp->adaptation_field_length = data[i++];
	dlog_log("adaptaion(%d)  flag: %0x\n", adp->adaptation_field_length, (unsigned int)data[i]);

	if(adp->adaptation_field_length > 0)
	{
		adp->discontinuity_indicator = (data[i] >> 7) & 0x01;
		adp->random_access_indicator = (data[i] >> 6) & 0x01;
		adp->elementary_stream_priority_indicator = (data[i] >> 5) & 0x01;
		adp->PCR_flag = (data[i] >> 4) & 0x01;
		adp->OPCR_flag = (data[i] >> 3) & 0x01;
		adp->splicing_point_flag = (data[i] >> 2) & 0x01;
		adp->transport_private_data_flag = (data[i] >> 1) & 0x01;
		adp->adaptation_field_extension_flag = (data[i] >> 0) & 0x01;
		i++;

		if(adp->PCR_flag)
		{
			adp->program_clock_reference_base = data[i];
			adp->program_clock_reference_base = (adp->program_clock_reference_base << 25) | (data[i+1] << 17) | (data[i+2] << 9) | (data[i+3] << 1) | ((data[i+4] >> 7) & 0x01);
			adp->program_clock_reference_extension = ((data[i+4] & 0x01) << 8) | data[i+5];

			i += 6;
		}

		if(adp->OPCR_flag)
		{
			adp->original_program_clock_reference_base = (data[i] << 25) | (data[i+1] << 17) | (data[i+2] << 9) | (data[i+3] << 1) | ((data[i+4] >> 7) & 0x01);
			adp->original_program_clock_reference_extension = ((data[i+4] & 0x01) << 1) | data[i+5];

			i += 6;
		}

		if(adp->splicing_point_flag)
		{
			adp->splice_countdown = data[i++];
		}

		if(adp->transport_private_data_flag)
		{
			int j = 0;
			adp->transport_private_data_length = data[i++];
			for(j = 0; j < adp->transport_private_data_length; j++)
			{
				uint8_t transport_private_data = data[i+j];
			}

			i += adp->transport_private_data_length;
		}

		if(adp->adaptation_field_extension_flag)
		{
			uint8_t reserved;
			adp->adaptation_field_extension_length = data[i++];
			adp->ltw_flag = (data[i] >> 7) & 0x01;
			adp->piecewise_rate_flag = (data[i] >> 6) & 0x01;
			adp->seamless_splice_flag = (data[i] >> 5) & 0x01;
			reserved = data[i] & 0x1F;

			i++;
			if(adp->ltw_flag)
			{
				uint8_t ltw_valid_flag = (data[i] >> 7) & 0x01;
				uint16_t ltw_offset = ((data[i] & 0x7F) << 8) | data[i+1];
				i += 2;
			}

			if(adp->piecewise_rate_flag)
			{
				uint32_t piecewise_rate = ((data[i] & 0x3F) << 16) | (data[i+1] << 8) | data[i+2];
				i += 3;
			}

			if(adp->seamless_splice_flag)
			{
				uint8_t Splice_type = (data[i] >> 4) & 0x0F;
				uint32_t DTS_next_AU = (((data[i] >> 1) & 0x07) << 30) | (data[i+1] << 22) | (((data[i+2] >> 1) & 0x7F) << 15) | (data[i+3] << 7) | ((data[i+4] >> 1) & 0x7F);

				i += 5;
			}

			// reserved byte
		}

		// stuffing byte
	}

	return adp->adaptation_field_length + 1;
}

#define TS_SYNC_BYTE(data)						(data[0] == 0x47)
#define TS_TRANSPORT_ERROR_INDICATOR(data)		(data[1] & 0x80)
#define TS_PAYLOAD_UNIT_START_INDICATOR(data)	(data[1] & 0x40)
#define TS_TRANSPORT_PRIORITY(data)				(data[1] & 0x20)

static char s_video[1024*1024];
static char s_audio[1024*1024];

int ts_packet_dec(const uint8_t* data, int bytes)
{
	int i, j, k;
	int64_t t;
	ts_packet_header_t pkhd;
	uint32_t PID;

	static FILE* s_decfp;
	if(!s_decfp)
		s_decfp = fopen("e:\\0.raw", "wb");

	// 2.4.3 Specification of the transport stream syntax and semantics
	// Transport stream packets shall be 188 bytes long.
	assert(188 == bytes);

	// 2.4.3.2 Transport stream packet layer
	// Table 2-2
    memset(&pkhd, 0, sizeof(pkhd));
	PID = ((data[1] << 8) | data[2]) & 0x1FFF;
	pkhd.transport_scrambling_control = (data[3] >> 6) & 0x03;
	pkhd.adaptation_field_control = (data[3] >> 4) & 0x03;
	pkhd.continuity_counter = data[3] & 0x0F;

	dlog_log("-----------------------------------------------\n");
	dlog_log("packet: %0x %0x %0x %0x\n", (unsigned int)data[0], (unsigned int)data[1], (unsigned int)data[2], (unsigned int)data[3]);

	i = 4;
	if(0x02 == pkhd.adaptation_field_control || 0x03 == pkhd.adaptation_field_control)
	{
		i += ts_packet_adaptation(data + 4, bytes - 4, &pkhd.adaptation);

		if(pkhd.adaptation.adaptation_field_length > 0 && pkhd.adaptation.PCR_flag)
		{
			t = pkhd.adaptation.program_clock_reference_base / 90L; // ms;
			dlog_log("pcr: %02d:%02d:%02d.%03d - %lld/%u\n", (int)(t / 3600000), (int)(t % 3600000)/60000, (int)((t/1000) % 60), (int)(t % 1000), pkhd.adaptation.program_clock_reference_base, pkhd.adaptation.program_clock_reference_extension);
		}
	}

	if(0x01 == pkhd.adaptation_field_control || 0x03 == pkhd.adaptation_field_control)
	{
		if(0x00 == PID)
		{
			if(TS_PAYLOAD_UNIT_START_INDICATOR(data))
				i += 1; // pointer 0x00

			tsctx.pat.pmt = tsctx.pmt;
			tsctx.pmt->streams = tsctx.pes;
			ts_pat_dec(data + i, bytes - i, &tsctx.pat);
		}
		else
		{
			for(j = 0; j < tsctx.pat.pmt_count; j++)
			{
				if(PID == tsctx.pat.pmt[j].pid)
				{
					if(TS_PAYLOAD_UNIT_START_INDICATOR(data))
						i += 1; // pointer 0x00

					ts_pmt_dec(data + i, bytes - i, &tsctx.pmt[j]);
					break;
				}

				for(k = 0; k < tsctx.pat.pmt[j].stream_count; k++)
				{
					if(PID == tsctx.pes[k].pid)
					{
						if(TS_PAYLOAD_UNIT_START_INDICATOR(data))
						{
							static int s_n = 0;
							if(s_n++ < 1500)
							{
								if(!tsctx.pes[k].payload)
									tsctx.pes[k].payload = (STREAM_VIDEO_H264==tsctx.pes[k].sid) ? s_video : s_audio;

								if(tsctx.pes[k].payload_len > 0)
								{
									fwrite(&tsctx.pes[k].sid, 1, 4, s_decfp);
									fwrite(&tsctx.pes[k].payload_len, 1, 4, s_decfp);
									fwrite(tsctx.pes[k].payload, 1, tsctx.pes[k].payload_len, s_decfp);
									fflush(s_decfp);

									tsctx.pes[k].payload_len = 0;
								}
							}
							else
							{
								fclose(s_decfp);
								exit(0);
							}

                            pes_dec(NULL, data + i, bytes - i, &tsctx.pes[k]);

                            if(0 == k)
                                dlog_log("pes payload: %u\n", tsctx.pes[k].len);

							if(tsctx.pes[k].PTS_DTS_flags & 0x02)
							{
								t = tsctx.pes[k].pts / 90;
								dlog_log("pts: %02d:%02d:%02d.%03d - %lld\n", (int)(t / 3600000), (int)(t % 3600000)/60000, (int)((t/1000) % 60), (int)(t % 1000), tsctx.pes[k].pts);
							}

							if(tsctx.pes[k].PTS_DTS_flags & 0x01)
							{
								t = tsctx.pes[k].dts / 90;
								dlog_log("dts: %02d:%02d:%02d.%03d - %lld\n", (int)(t / 3600000), (int)(t % 3600000)/60000, (int)((t/1000) % 60), (int)(t % 1000), tsctx.pes[k].dts);
							}
						}
						else
						{
                            memcpy(tsctx.pes[k].payload + tsctx.pes[k].payload_len, data + i, bytes - i);
                            tsctx.pes[k].payload_len += bytes - i;

                            if(tsctx.pes[i].len > 0)
                                tsctx.pes[k].len -= bytes - i;
						}

						break; // goto out?
					}
				}
			}
		}
	}

	return 0;
}

static int mpeg_ts_is_idr_first_packet(const void* packet, int bytes)
{
	unsigned char *data;
	ts_packet_header_t pkhd;
	int payload_unit_start_indicator;

	memset(&pkhd, 0, sizeof(pkhd));

	data = (const unsigned char *)packet;
	payload_unit_start_indicator = data[1] & 0x40;
	pkhd.adaptation_field_control = (data[3] >> 4) & 0x03;
	pkhd.continuity_counter = data[3] & 0x0F;

	if(0x02 == pkhd.adaptation_field_control || 0x03 == pkhd.adaptation_field_control)
	{
		ts_packet_adaptation(data + 4, bytes - 4, &pkhd.adaptation);
	}

	return (payload_unit_start_indicator && pkhd.adaptation.random_access_indicator) ? 1 : 0;
}
