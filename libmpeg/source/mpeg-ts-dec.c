// ITU-T H.222.0(06/2012)
// Information technology - Generic coding of moving pictures and associated audio information: Systems
// 2.4.3.1 Transport stream(p34)

#include "mpeg-pes-internal.h"
#include "mpeg-ts-internal.h"
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
	uint32_t end;
	uint32_t extension_end;
	uint32_t required;

	assert(bytes <= TS_PACKET_SIZE);
	if (bytes < 1)
		return 0;
	adp->adaptation_field_length = data[i++];
	end = adp->adaptation_field_length + 1;
	if (end > bytes)
		return 0;
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
		required = (adp->PCR_flag ? 6 : 0)
			+ (adp->OPCR_flag ? 6 : 0)
			+ (adp->splicing_point_flag ? 1 : 0);
		if (i + required > end)
			return 0;

		if(adp->PCR_flag)
		{
			adp->program_clock_reference_base = ((uint64_t)data[i] << 25) | ((uint64_t)data[i+1] << 17) | ((uint64_t)data[i+2] << 9) | ((uint64_t)data[i+3] << 1) | ((data[i+4] >> 7) & 0x01);
			adp->program_clock_reference_extension = ((data[i+4] & 0x01) << 8) | data[i+5];

			i += 6;
		}

		if(adp->OPCR_flag)
		{
			adp->original_program_clock_reference_base = (((uint64_t)data[i]) << 25) | ((uint64_t)data[i+1] << 17) | ((uint64_t)data[i+2] << 9) | ((uint64_t)data[i+3] << 1) | ((data[i+4] >> 7) & 0x01);
			adp->original_program_clock_reference_extension = ((data[i+4] & 0x01) << 8) | data[i+5];

			i += 6;
		}

		if(adp->splicing_point_flag)
		{
			adp->splice_countdown = data[i++];
		}

		if(adp->transport_private_data_flag)
		{
			if (i + 1 > end)
				return 0;
			adp->transport_private_data_length = data[i++];
			if (i + adp->transport_private_data_length > end)
				return 0;
			i += adp->transport_private_data_length;
		}

		if(adp->adaptation_field_extension_flag)
		{
			if (i + 1 > end)
				return 0;
			//uint8_t reserved;
			adp->adaptation_field_extension_length = data[i++];
			if (i + adp->adaptation_field_extension_length > end)
				return 0;
			extension_end = i + adp->adaptation_field_extension_length;
			if (i == extension_end)
				return end;
			adp->ltw_flag = (data[i] >> 7) & 0x01;
			adp->piecewise_rate_flag = (data[i] >> 6) & 0x01;
			adp->seamless_splice_flag = (data[i] >> 5) & 0x01;
			//reserved = data[i] & 0x1F;

			i++;
			required = (adp->ltw_flag ? 2 : 0)
				+ (adp->piecewise_rate_flag ? 3 : 0)
				+ (adp->seamless_splice_flag ? 5 : 0);
			if (i + required > extension_end)
				return 0;
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
			i = extension_end;
		}

		// stuffing byte
	}

	return end;
}

#define TS_IS_SYNC_BYTE(data)					(data[0] == TS_SYNC_BYTE)
#define TS_TRANSPORT_ERROR_INDICATOR(data)		(data[1] & 0x80)
#define TS_PAYLOAD_UNIT_START_INDICATOR(data)	(data[1] & 0x40)
#define TS_TRANSPORT_PRIORITY(data)				(data[1] & 0x20)

#define PES_HEADER_MIN_SIZE 9
#define PES_HEADER_MAX_SIZE (PES_HEADER_MIN_SIZE + 255)
#define PES_PAYLOAD_UNBOUNDED UINT32_MAX

static struct pes_t* ts_demuxer_find_pes(struct ts_demuxer_t* ts, uint32_t pid)
{
	uint32_t i, j;
	for (i = 0; i < ts->pat.pmt_count; ++i)
	{
		for (j = 0; j < ts->pat.pmts[i].stream_count; ++j)
		{
			if (pid == ts->pat.pmts[i].streams[j].pid)
				return &ts->pat.pmts[i].streams[j];
		}
	}
	return NULL;
}

static void ts_demuxer_reset_pes_header(struct pes_t* pes)
{
	pes->have_pes_header = 0;
	pes->ts_pes_payload_remaining = 0;
	pes->ts_pes_header_bytes = 0;
	pes->ts_pes_payload_start = 0;
}

static int ts_demuxer_read_pes_header(struct pes_t* pes, const uint8_t* data,
	size_t bytes, size_t* consume)
{
	int r;
	size_t n;
	size_t length;
	size_t prefix;
	uint16_t packet_length;
	const uint8_t* header;
	struct mpeg_bits_t reader;

	*consume = 0;
	header = NULL;
	if (0 == pes->ts_pes_header_bytes && bytes >= 4
		&& (0x00 != data[0] || 0x00 != data[1] || 0x01 != data[2]))
		return MPEG_ERROR_INVALID_DATA;

	if (0 == pes->ts_pes_header_bytes && bytes >= PES_HEADER_MIN_SIZE)
	{
		// Keep the normal path copy-free; only split headers use the cache.
		// 正常路径直接解析，只有跨包的 PES 头才进入缓存。
		length = PES_HEADER_MIN_SIZE + data[8];
		if (bytes >= length)
		{
			header = data;
			*consume = length;
		}
	}

	while (NULL == header && *consume < bytes)
	{
		length = pes->ts_pes_header_bytes < PES_HEADER_MIN_SIZE
			? PES_HEADER_MIN_SIZE
			: PES_HEADER_MIN_SIZE + pes->ts_pes_header[8];
		assert(length <= PES_HEADER_MAX_SIZE);
		n = length - pes->ts_pes_header_bytes;
		if (n > bytes - *consume)
			n = bytes - *consume;
		memcpy(pes->ts_pes_header + pes->ts_pes_header_bytes, data + *consume, n);
		pes->ts_pes_header_bytes += (uint16_t)n;
		*consume += n;

		if (pes->ts_pes_header_bytes >= 4
			&& (0x00 != pes->ts_pes_header[0]
				|| 0x00 != pes->ts_pes_header[1]
				|| 0x01 != pes->ts_pes_header[2]))
			return MPEG_ERROR_INVALID_DATA;

		if (pes->ts_pes_header_bytes >= PES_HEADER_MIN_SIZE)
		{
			length = PES_HEADER_MIN_SIZE + pes->ts_pes_header[8];
			if (pes->ts_pes_header_bytes >= length)
				header = pes->ts_pes_header;
		}
	}

	if (NULL == header)
		return MPEG_ERROR_NEED_MORE_DATA;

	packet_length = (uint16_t)(((uint16_t)header[4] << 8) | header[5]);
	mpeg_bits_init(&reader, header, PES_HEADER_MIN_SIZE + header[8]);
	prefix = mpeg_bits_readn(&reader, 3);
	pes->sid = mpeg_bits_read8(&reader);
	r = pes_read_header(pes, &reader);
	if (MPEG_ERROR_OK != r || 0x000001 != prefix)
		return MPEG_ERROR_INVALID_DATA;

	pes->ts_pes_header_bytes = 0;
	// A zero PES_packet_length extends to the next PUSI.
	// PES_packet_length 为零时，当前 PES 持续到下一个 PUSI。
	pes->ts_pes_payload_remaining = 0 == packet_length
		? PES_PAYLOAD_UNBOUNDED
		: pes->len;
	pes->ts_pes_payload_start = 1;
	pes->have_pes_header = 1;
	return MPEG_ERROR_OK;
}

int ts_demuxer_flush(struct ts_demuxer_t* ts)
{
    int r;
    uint32_t i, j;
	size_t consume;
    for (i = 0; i < ts->pat.pmt_count; i++)
    {
        for (j = 0; j < ts->pat.pmts[i].stream_count; j++)
        {
            struct pes_t* pes = &ts->pat.pmts[i].streams[j];
            if (pes->pkt.size < 5
                && PSI_STREAM_MPEG1 != pes->codecid && PSI_STREAM_MPEG2 != pes->codecid)
			{
				ts_demuxer_reset_pes_header(pes);
                continue;
			}
            
            if (PSI_STREAM_H264 == pes->codecid)
            {
                const uint8_t aud[] = {0,0,0,1,0x09,0xf0};
                r = pes_packet(&pes->pkt, pes, aud, sizeof(aud), &consume, 0, ts->onpacket, ts->param);
            }
            else if (PSI_STREAM_H265 == pes->codecid)
            {
                const uint8_t aud[] = {0,0,0,1,0x46,0x01,0x50};
                r = pes_packet(&pes->pkt, pes, aud, sizeof(aud), &consume, 0, ts->onpacket, ts->param);
            }
			else if (PSI_STREAM_H266 == pes->codecid)
			{
				const uint8_t aud[] = { 0,0,0,1,0x00,0xA1,0x18 };
				r = pes_packet(&pes->pkt, pes, aud, sizeof(aud), &consume, 0, ts->onpacket, ts->param);
			}
            else
            {
                //assert(0);
                r = pes_packet(&pes->pkt, pes, NULL, 0, &consume, 0, ts->onpacket, ts->param);
            }

			ts_demuxer_reset_pes_header(pes);
            if (0 != r)
                return r;
        }
    }
    return 0;
}

int ts_demuxer_input(struct ts_demuxer_t* ts, const uint8_t* data, size_t bytes)
{
    int r = 0;
	uint32_t i, j, k;
	uint32_t PID;
	size_t consume, payload;
	unsigned int count, ver;
    struct ts_packet_header_t pkhd;
	struct pes_t* error_pes;

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
		j = adaptation_filed_read(&pkhd.adaptation, data + 4, bytes - 4);
		if (0 == j)
		{
			error_pes = ts_demuxer_find_pes(ts, PID);
			if (error_pes)
			{
				error_pes->flags |= MPEG_FLAG_PACKET_CORRUPT | MPEG_FLAG_PACKET_LOST;
				ts_demuxer_reset_pes_header(error_pes);
				++error_pes->discontinuity;
				error_pes->cc = (uint8_t)pkhd.continuity_counter;
			}
			return 0;
		}
		i += j;

		if(pkhd.adaptation.adaptation_field_length > 0 && pkhd.adaptation.PCR_flag)
		{
            //int64_t t;
			//t = pkhd.adaptation.program_clock_reference_base / 90L; // ms;
			//printf("pcr: %02d:%02d:%02d.%03d - %" PRId64 "/%u\n", (int)(t / 3600000), (int)(t % 3600000)/60000, (int)((t/1000) % 60), (int)(t % 1000), pkhd.adaptation.program_clock_reference_base, pkhd.adaptation.program_clock_reference_extension);
		}
	}

	error_pes = NULL;
	if (pkhd.transport_error_indicator || pkhd.adaptation.discontinuity_indicator)
	{
		error_pes = ts_demuxer_find_pes(ts, PID);
		if (error_pes)
		{
			if (pkhd.transport_error_indicator)
			{
				error_pes->flags |= MPEG_FLAG_PACKET_CORRUPT | MPEG_FLAG_PACKET_LOST;
				ts_demuxer_reset_pes_header(error_pes);
			}
			else if (error_pes->ts_pes_header_bytes > 0)
			{
				ts_demuxer_reset_pes_header(error_pes);
			}
			++error_pes->discontinuity;
			error_pes->cc = (uint8_t)pkhd.continuity_counter;
		}
	}

	if (pkhd.transport_error_indicator)
		return 0; // discard payload with uncorrectable transport errors

	if (0x02 & pkhd.adaptation_field_control)
	{
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

					ver = ts->pat.pmts[j].ver;
					count = ts->pat.pmts[j].stream_count;
					pmt_read(&ts->pat.pmts[j], data + i, bytes - i);
					if(ver != ts->pat.pmts[j].ver || count != ts->pat.pmts[j].stream_count)
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

						if (!pkhd.adaptation.discontinuity_indicator
							&& ((ts->pat.pmts[j].streams[k].cc + 1) % 16) != (uint8_t)pkhd.continuity_counter)
							{
								pes->flags |= MPEG_FLAG_PACKET_CORRUPT | MPEG_FLAG_PACKET_LOST;
								++pes->discontinuity;
								if (!pkhd.payload_unit_start_indicator
									&& pes->ts_pes_header_bytes > 0)
									ts_demuxer_reset_pes_header(pes);
								else if (!pkhd.payload_unit_start_indicator
									&& pes->have_pes_header
									&& PES_PAYLOAD_UNBOUNDED != pes->ts_pes_payload_remaining)
								{
									// Lost payload makes the finite PES boundary unknown.
									// 载荷丢失后无法再确定有限 PES 的精确结束位置。
									pes->ts_pes_payload_remaining = PES_PAYLOAD_UNBOUNDED;
									pes->len = 0;
								}
							}
							ts->pat.pmts[j].streams[k].cc = (uint8_t)pkhd.continuity_counter;

							if (pkhd.payload_unit_start_indicator)
								ts_demuxer_reset_pes_header(pes);

							if (pkhd.payload_unit_start_indicator
								|| pes->ts_pes_header_bytes > 0)
							{
								r = ts_demuxer_read_pes_header(pes, data + i,
									bytes - i, &consume);
								i += (uint32_t)consume;
								if (MPEG_ERROR_NEED_MORE_DATA == r)
									return 0;
								if (MPEG_ERROR_OK != r)
								{
									ts_demuxer_reset_pes_header(pes);
									return 0;
								}

								pes->flags = (pes->flags & MPEG_FLAG_PACKET_CORRUPT) ? MPEG_FLAG_PACKET_LOST : 0;
								pes->flags |= pes->data_alignment_indicator ? MPEG_FLAG_IDR_FRAME : 0;
							}
							else if (!pes->have_pes_header)
							{
								return 0; // ignore, don't have pes header yet
							}

							if (0 == pes->ts_pes_payload_remaining)
							{
								ts_demuxer_reset_pes_header(pes);
								return 0;
							}

							payload = bytes - i;
							if (PES_PAYLOAD_UNBOUNDED != pes->ts_pes_payload_remaining
								&& payload > pes->ts_pes_payload_remaining)
								payload = pes->ts_pes_payload_remaining;
							if (0 == payload)
								return 0;

	                        r = pes_packet(&pes->pkt, pes, data + i, payload, &consume,
								pes->ts_pes_payload_start, ts->onpacket, ts->param);
							pes->ts_pes_payload_start = 0;
							if (0 != r)
							{
								ts_demuxer_reset_pes_header(pes);
								return r;
							}
							if (PES_PAYLOAD_UNBOUNDED != pes->ts_pes_payload_remaining)
							{
								assert(payload <= pes->ts_pes_payload_remaining);
								pes->ts_pes_payload_remaining -= (uint32_t)payload;
								if (0 == pes->ts_pes_payload_remaining)
									pes->have_pes_header = 0;
							}
	                        return r; // find stream
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
		if (0 == adaptation_filed_read(&pkhd.adaptation, data + 4, bytes - 4))
			return 0;
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
	pat_clear(&ts->pat);
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
