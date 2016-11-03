// ITU-T H.222.0(06/2012)
// Information technology ¨C Generic coding of moving pictures and associated audio information: Systems
// 2.4.3.1 Transport stream(p34)

#include "mpeg-ts-proto.h"
#include "mpeg-ps-proto.h"
#include "mpeg-pes-proto.h"
#include "mpeg-ts.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>

struct mpeg_ts_handler_t
{
	void(*handler)();
};

typedef struct _mpeg_ts_dec_context_t
{
	pat_t pat;
	pmt_t pmt[1];
	pes_t pes[2];
	struct mpeg_ts_handler_t handlers[0x1fff + 1]; // TODO: setup PID handler

	uint8_t payload[4 * 1024 * 1024]; // TODO: need more payload buffer!!!
} mpeg_ts_dec_context_t;

static mpeg_ts_dec_context_t tsctx;

static uint32_t ts_packet_adaptation(const uint8_t* data, size_t bytes, ts_adaptation_field_t *adp)
{
	// 2.4.3.4 Adaptation field
	// Table 2-6
	uint32_t i = 0;
	uint32_t j = 0;

	assert(bytes <= TS_PACKET_SIZE);
	adp->adaptation_field_length = data[i++];
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
			adp->program_clock_reference_base = ((uint64_t)data[i] << 25) | ((uint64_t)data[i+1] << 17) | ((uint64_t)data[i+2] << 9) | ((uint64_t)data[i+3] << 1) | ((data[i+4] >> 7) & 0x01);
			adp->program_clock_reference_extension = ((data[i+4] & 0x01) << 8) | data[i+5];

			i += 6;
		}

		if(adp->OPCR_flag)
		{
			adp->original_program_clock_reference_base = (((uint64_t)data[i]) << 25) | ((uint64_t)data[i+1] << 17) | ((uint64_t)data[i+2] << 9) | ((uint64_t)data[i+3] << 1) | ((data[i+4] >> 7) & 0x01);
			adp->original_program_clock_reference_extension = ((data[i+4] & 0x01) << 1) | data[i+5];

			i += 6;
		}

		if(adp->splicing_point_flag)
		{
			adp->splice_countdown = data[i++];
		}

		if(adp->transport_private_data_flag)
		{
			adp->transport_private_data_length = data[i++];
			for(j = 0; j < adp->transport_private_data_length; j++)
			{
//				uint8_t transport_private_data = data[i+j];
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
//				uint8_t ltw_valid_flag = (data[i] >> 7) & 0x01;
//				uint16_t ltw_offset = ((data[i] & 0x7F) << 8) | data[i+1];
				i += 2;
			}

			if(adp->piecewise_rate_flag)
			{
//				uint32_t piecewise_rate = ((data[i] & 0x3F) << 16) | (data[i+1] << 8) | data[i+2];
				i += 3;
			}

			if(adp->seamless_splice_flag)
			{
//				uint8_t Splice_type = (data[i] >> 4) & 0x0F;
//				uint32_t DTS_next_AU = (((data[i] >> 1) & 0x07) << 30) | (data[i+1] << 22) | (((data[i+2] >> 1) & 0x7F) << 15) | (data[i+3] << 7) | ((data[i+4] >> 1) & 0x7F);

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

static uint8_t s_video[1024*1024];
static uint8_t s_audio[1024*1024];

int mpeg_ts_packet_dec(const uint8_t* data, size_t bytes, onpacket handler, void* param)
{
	uint32_t i, j, k;
	int64_t t;
	psm_t psm;
	ts_packet_header_t pkhd;
	uint32_t PID;

	// 2.4.3 Specification of the transport stream syntax and semantics
	// Transport stream packets shall be 188 bytes long.
	assert(188 == bytes);

	// 2.4.3.2 Transport stream packet layer
	// Table 2-2
    memset(&pkhd, 0, sizeof(pkhd));
	assert(0x47 == data[0]); // sync_byte
	PID = ((data[1] << 8) | data[2]) & 0x1FFF;
	pkhd.transport_error_indicator = (data[1] >> 7) & 0x01;
	pkhd.payload_unit_start_indicator = (data[1] >> 6) & 0x01;
	pkhd.transport_priority = (data[1] >> 5) & 0x01;
	pkhd.transport_scrambling_control = (data[3] >> 6) & 0x03;
	pkhd.adaptation_field_control = (data[3] >> 4) & 0x03;
	pkhd.continuity_counter = data[3] & 0x0F;

//	printf("-----------------------------------------------\n");
//	printf("PID[%u]: Start:%u, Priority:%u, Scrambler:%u, AF: %u, CC: %u\n", PID, pkhd.payload_unit_start_indicator, pkhd.transport_priority, pkhd.transport_scrambling_control, pkhd.adaptation_field_control, pkhd.continuity_counter);

	i = 4;
	if(0x02 & pkhd.adaptation_field_control)
	{
		i += ts_packet_adaptation(data + 4, bytes - 4, &pkhd.adaptation);

		if(pkhd.adaptation.adaptation_field_length > 0 && pkhd.adaptation.PCR_flag)
		{
			t = pkhd.adaptation.program_clock_reference_base / 90L; // ms;
			printf("pcr: %02d:%02d:%02d.%03d - %" PRId64 "/%u\n", (int)(t / 3600000), (int)(t % 3600000)/60000, (int)((t/1000) % 60), (int)(t % 1000), pkhd.adaptation.program_clock_reference_base, pkhd.adaptation.program_clock_reference_extension);
		}
	}

	if(0x01 & pkhd.adaptation_field_control)
	{
		if(0x00 == PID)
		{
			if(TS_PAYLOAD_UNIT_START_INDICATOR(data))
				i += 1; // pointer 0x00

			tsctx.pat.pmt = tsctx.pmt;
			tsctx.pmt->streams = tsctx.pes;
			pat_read(data + i, bytes - i, &tsctx.pat);
		}
		else
		{
			for(j = 0; j < tsctx.pat.pmt_count; j++)
			{
				if(PID == tsctx.pat.pmt[j].pid)
				{
					if(TS_PAYLOAD_UNIT_START_INDICATOR(data))
						i += 1; // pointer 0x00

					pmt_read(data + i, bytes - i, &tsctx.pmt[j]);
					break;
				}
				else
				{
					for (k = 0; k < tsctx.pat.pmt[j].stream_count; k++)
					{
						if (PID == tsctx.pes[k].pid)
						{
							if (TS_PAYLOAD_UNIT_START_INDICATOR(data))
							{
								if (!tsctx.pes[k].payload)
									tsctx.pes[k].payload = (PSI_STREAM_H264 == tsctx.pes[k].avtype) ? s_video : s_audio;

								if (tsctx.pes[k].payload_len > 0)
								{
									assert(0 == tsctx.pes[k].len || tsctx.pes[k].payload_len == tsctx.pes[k].len - tsctx.pes[k].PES_header_data_length - 3);
									// TODO: filter 0x09 AUD
									if ((tsctx.pes[k].payload[4] == 0x09 && 0x00 == tsctx.pes[k].payload[0] && 0x00 == tsctx.pes[k].payload[1] && 0x00 == tsctx.pes[k].payload[2] && 0x01 == tsctx.pes[k].payload[3]))
										handler(param, tsctx.pes[k].avtype, tsctx.pes[k].pts, tsctx.pes[k].dts, tsctx.pes[k].payload + 6, tsctx.pes[k].payload_len - 6);
									else
										handler(param, tsctx.pes[k].avtype, tsctx.pes[k].pts, tsctx.pes[k].dts, tsctx.pes[k].payload, tsctx.pes[k].payload_len);

									tsctx.pes[k].payload_len = 0;
								}

								pes_read(data + i, bytes - i, &psm, &tsctx.pes[k]);
							}
							else
							{
								memcpy(tsctx.pes[k].payload + tsctx.pes[k].payload_len, data + i, bytes - i);
								tsctx.pes[k].payload_len += bytes - i;

								//if(tsctx.pes[i].len > 0)
								//    tsctx.pes[k].len -= bytes - i;
							}

							break; // find stream
						}
					}
				} // PMT handler
			}
		} // PAT handler
	}

	return 0;
}

static inline int mpeg_ts_is_idr_first_packet(const void* packet, int bytes)
{
	const unsigned char *data;
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
