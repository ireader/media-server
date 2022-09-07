// ITU-T H.222.0(06/2012)
// Information technology - Generic coding of moving pictures and associated audio information: Systems
// 2.4.3.1 Transport stream(p34)

#include "mpeg-ts-proto.h"
#include "mpeg-ps-proto.h"
#include "mpeg-pes-proto.h"
#include "mpeg-util.h"
#include "mpeg-ts.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>

struct ts_demuxer_t
{
    struct pat_t pat;

    ts_demuxer_onpacket onpacket;
    void* param;

	struct ts_demuxer_notify_t notify;
	void* notify_param;
};

static void ts_demuxer_notify(struct ts_demuxer_t* ts, const struct pmt_t* pmt);

static uint32_t adaptation_filed_read(struct ts_adaptation_field_t *adp, const uint8_t* data, size_t bytes)
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

		if(adp->PCR_flag && i + 6 <= adp->adaptation_field_length + 1)
		{
			adp->program_clock_reference_base = ((uint64_t)data[i] << 25) | ((uint64_t)data[i+1] << 17) | ((uint64_t)data[i+2] << 9) | ((uint64_t)data[i+3] << 1) | ((data[i+4] >> 7) & 0x01);
			adp->program_clock_reference_extension = ((data[i+4] & 0x01) << 8) | data[i+5];

			i += 6;
		}

		if(adp->OPCR_flag && i + 6 <= adp->adaptation_field_length + 1)
		{
			adp->original_program_clock_reference_base = (((uint64_t)data[i]) << 25) | ((uint64_t)data[i+1] << 17) | ((uint64_t)data[i+2] << 9) | ((uint64_t)data[i+3] << 1) | ((data[i+4] >> 7) & 0x01);
			adp->original_program_clock_reference_extension = ((data[i+4] & 0x01) << 1) | data[i+5];

			i += 6;
		}

		if(adp->splicing_point_flag && i + 1 <= adp->adaptation_field_length + 1)
		{
			adp->splice_countdown = data[i++];
		}

		if(adp->transport_private_data_flag && i + 1 <= adp->adaptation_field_length + 1)
		{
			adp->transport_private_data_length = data[i++];
			for(j = 0; j < adp->transport_private_data_length; j++)
			{
//				uint8_t transport_private_data = data[i+j];
			}

			i += adp->transport_private_data_length;
		}

		if(adp->adaptation_field_extension_flag && i + 2 <= adp->adaptation_field_length + 1)
		{
			//uint8_t reserved;
			adp->adaptation_field_extension_length = data[i++];
			adp->ltw_flag = (data[i] >> 7) & 0x01;
			adp->piecewise_rate_flag = (data[i] >> 6) & 0x01;
			adp->seamless_splice_flag = (data[i] >> 5) & 0x01;
			//reserved = data[i] & 0x1F;

			i++;
			if(adp->ltw_flag && i + 2 <= adp->adaptation_field_length + 1)
			{
//				uint8_t ltw_valid_flag = (data[i] >> 7) & 0x01;
//				uint16_t ltw_offset = ((data[i] & 0x7F) << 8) | data[i+1];
				i += 2;
			}

			if(adp->piecewise_rate_flag && i + 3 <= adp->adaptation_field_length + 1)
			{
//				uint32_t piecewise_rate = ((data[i] & 0x3F) << 16) | (data[i+1] << 8) | data[i+2];
				i += 3;
			}

			if(adp->seamless_splice_flag && i + 5 <= adp->adaptation_field_length + 1)
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

#define TS_IS_SYNC_BYTE(data)					(data[0] == TS_SYNC_BYTE)
#define TS_TRANSPORT_ERROR_INDICATOR(data)		(data[1] & 0x80)
#define TS_PAYLOAD_UNIT_START_INDICATOR(data)	(data[1] & 0x40)
#define TS_TRANSPORT_PRIORITY(data)				(data[1] & 0x20)

int ts_demuxer_flush(struct ts_demuxer_t* ts)
{
    uint32_t i, j;
    for (i = 0; i < ts->pat.pmt_count; i++)
    {
        for (j = 0; j < ts->pat.pmts[i].stream_count; j++)
        {
            struct pes_t* pes = &ts->pat.pmts[i].streams[j];
            if (pes->pkt.size < 5)
                continue;
            
            if (PSI_STREAM_H264 == pes->codecid)
            {
                const uint8_t aud[] = {0,0,0,1,0x09,0xf0};
                pes_packet(&pes->pkt, pes, aud, sizeof(aud), 0, ts->onpacket, ts->param);
            }
            else if (PSI_STREAM_H265 == pes->codecid)
            {
                const uint8_t aud[] = {0,0,0,1,0x46,0x01,0x50};
                pes_packet(&pes->pkt, pes, aud, sizeof(aud), 0, ts->onpacket, ts->param);
            }
            else
            {
                //assert(0);
                pes_packet(&pes->pkt, pes, NULL, 0, 0, ts->onpacket, ts->param);
            }
        }
    }
    return 0;
}

int ts_demuxer_input(struct ts_demuxer_t* ts, const uint8_t* data, size_t bytes)
{
    int r = 0;
    uint32_t i, j, k;
	uint32_t PID;
	unsigned int count;
    struct ts_packet_header_t pkhd;

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
//	printf("PID[%u]: Error: %u, Start:%u, Priority:%u, Scrambler:%u, AF: %u, CC: %u\n", PID, pkhd.transport_error_indicator, pkhd.payload_unit_start_indicator, pkhd.transport_priority, pkhd.transport_scrambling_control, pkhd.adaptation_field_control, pkhd.continuity_counter);

	i = 4;
	if(0x02 & pkhd.adaptation_field_control)
	{
		i += adaptation_filed_read(&pkhd.adaptation, data + 4, bytes - 4);

		if(pkhd.adaptation.adaptation_field_length > 0 && pkhd.adaptation.PCR_flag)
		{
            //int64_t t;
			//t = pkhd.adaptation.program_clock_reference_base / 90L; // ms;
			//printf("pcr: %02d:%02d:%02d.%03d - %" PRId64 "/%u\n", (int)(t / 3600000), (int)(t % 3600000)/60000, (int)((t/1000) % 60), (int)(t % 1000), pkhd.adaptation.program_clock_reference_base, pkhd.adaptation.program_clock_reference_extension);
		}

		assert(i <= bytes);
		if (i >= bytes)
			return 0; // ignore
	}
    
	if(0x01 & pkhd.adaptation_field_control)
	{
		if(TS_PID_PAT == PID)
		{
			if(pkhd.payload_unit_start_indicator)
				i += 1; // pointer 0x00

			// TODO: PAT lost
			pat_read(&ts->pat, data + i, bytes - i);
		}
        else if(TS_PID_SDT == PID)
        {
            if(pkhd.payload_unit_start_indicator)
                i += 1; // pointer 0x00
            sdt_read(&ts->pat, data + i, bytes - i);
        }
		else
		{
			for(j = 0; j < ts->pat.pmt_count; j++)
			{
				if(PID == ts->pat.pmts[j].pid)
				{
					// TODO: PMT lost
					if(pkhd.payload_unit_start_indicator)
						i += 1; // pointer 0x00

					count = ts->pat.pmts[j].stream_count;
					pmt_read(&ts->pat.pmts[j], data + i, bytes - i);
					if(count != ts->pat.pmts[j].stream_count)
						ts_demuxer_notify(ts, &ts->pat.pmts[j]);
					break;
				}
				else
				{
					for (k = 0; k < ts->pat.pmts[j].stream_count; k++)
					{
                        struct pes_t* pes = &ts->pat.pmts[j].streams[k];
						if (PID != pes->pid)
                            continue;

						pes->flags |= ((ts->pat.pmts[j].streams[k].cc + 1) % 16) != (uint8_t)pkhd.continuity_counter ? (MPEG_FLAG_PACKET_CORRUPT | MPEG_FLAG_PACKET_LOST) : 0;
						ts->pat.pmts[j].streams[k].cc = (uint8_t)pkhd.continuity_counter;

                        if (pkhd.payload_unit_start_indicator)
                        {
                            size_t n;
                            n = pes_read_header(pes, data + i, bytes - i);
                            assert(n > 0);
                            i += (uint32_t)n;

							pes->flags = (pes->flags & MPEG_FLAG_PACKET_CORRUPT) ? MPEG_FLAG_PACKET_LOST : 0;
							pes->flags |= pes->data_alignment_indicator ? MPEG_FLAG_IDR_FRAME : 0;
							pes->have_pes_header = n > 0 ? 1 : 0;
						}
						else if (!pes->have_pes_header)
						{
							continue; // don't have pes header yet
						}

                        r = pes_packet(&pes->pkt, pes, data + i, bytes - i, pkhd.payload_unit_start_indicator, ts->onpacket, ts->param);
						pes->have_pes_header = (r || (0 == pes->pkt.size && pes->len > 0)) ? 0 : 1; // packet completed
                        break; // find stream
					}
				} // PMT handler
			}
		} // PAT handler
	}

	return r;
}

static inline int mpeg_ts_is_idr_first_packet(const void* packet, int bytes)
{
	const unsigned char *data;
	struct ts_packet_header_t pkhd;
	int payload_unit_start_indicator;

	memset(&pkhd, 0, sizeof(pkhd));

	data = (const unsigned char *)packet;
	payload_unit_start_indicator = data[1] & 0x40;
	pkhd.adaptation_field_control = (data[3] >> 4) & 0x03;
	pkhd.continuity_counter = data[3] & 0x0F;

	if(0x02 == pkhd.adaptation_field_control || 0x03 == pkhd.adaptation_field_control)
	{
		adaptation_filed_read(&pkhd.adaptation, data + 4, bytes - 4);
	}

	return (payload_unit_start_indicator && pkhd.adaptation.random_access_indicator) ? 1 : 0;
}

struct ts_demuxer_t* ts_demuxer_create(ts_demuxer_onpacket onpacket, void* param)
{
    struct ts_demuxer_t* ts;
    ts = calloc(1, sizeof(struct ts_demuxer_t));
    if (!ts)
        return NULL;

    ts->onpacket = onpacket;
    ts->param = param;
    return ts;
}

int ts_demuxer_destroy(struct ts_demuxer_t* ts)
{
    size_t i, j;
    struct pes_t* pes;
    for (i = 0; i < ts->pat.pmt_count; i++)
    {
        for (j = 0; j < ts->pat.pmts[i].stream_count; j++)
        {
            pes = &ts->pat.pmts[i].streams[j];
            if (pes->pkt.data)
                free(pes->pkt.data);
            pes->pkt.data = NULL;
        }
    }

	if (ts->pat.pmts && ts->pat.pmts != ts->pat.pmt_default)
		free(ts->pat.pmts);

    free(ts);
    return 0;
}

int ts_demuxer_getservice(struct ts_demuxer_t* ts, int program, char* provider, int nprovider, char* name, int nname)
{
    struct pmt_t* pmt;
    pmt = pat_find(&ts->pat, (uint16_t)program);
    if(NULL == pmt)
        return -1;
    
    snprintf(provider, nprovider, "%s", pmt->provider);
    snprintf(name, nname, "%s", pmt->name);
    return 0;
}

void ts_demuxer_set_notify(struct ts_demuxer_t* ts, struct ts_demuxer_notify_t* notify, void* param)
{
	ts->notify_param = param;
	memcpy(&ts->notify, notify, sizeof(ts->notify));
}

static void ts_demuxer_notify(struct ts_demuxer_t* ts, const struct pmt_t* pmt)
{
	unsigned int i;
	const struct pes_t* pes;
	if (!ts->notify.onstream)
		return;

	for (i = 0; i < pmt->stream_count; i++)
	{
		pes = &pmt->streams[i];
		ts->notify.onstream(ts->notify_param, pes->pid, pes->codecid, pes->esinfo, pes->esinfo_len, i + 1 >= pmt->stream_count ? 1 : 0);
	}
}
