// ITU-T H.222.0(06/2012)
// Information technology ¨C Generic coding of moving pictures and associated audio information: Systems
// 2.4.3.1 Transport stream(p34)

#include "mpeg-ts-proto.h"
#include "mpeg-ps-proto.h"
#include "mpeg-pes-proto.h"
#include "mpeg-util.h"
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
    struct pat_t pat;
	struct mpeg_ts_handler_t handlers[0x1fff + 1]; // TODO: setup PID handler

    struct
    {
        int flags;
        int codecid;
        
        int64_t pts;
        int64_t dts;

        uint8_t data[2*1024*1024];
        size_t size;
    } video, audio;

	uint8_t payload[4 * 1024 * 1024]; // TODO: need more payload buffer!!!
} mpeg_ts_dec_context_t;

static mpeg_ts_dec_context_t tsctx;

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

static uint8_t s_video[2*1024*1024];
static uint8_t s_audio[1024*1024];

static int mpeg_ts_packet_submit(struct pes_t* pes, onpacket handler, void* param)
{
    const uint8_t* p = pes->pkt.data;

    if (PSI_STREAM_H264 == pes->codecid)
    {
        if (tsctx.video.size > 0)
        {
            int aud = find_h264_access_unit_delimiter(pes->pkt.data, pes->pkt.size);
            if (-1 != aud)
            {
                p += aud;

                // only one AUD
                assert(-1 == find_h264_access_unit_delimiter(pes->pkt.data + aud + 4, pes->pkt.size - aud - 4));

                // merge data
                memcpy(tsctx.video.data + tsctx.video.size, pes->pkt.data, aud);
                tsctx.video.size += aud;

                //assert(0 == pes->len || pes->payload.len == pes->len);
                aud = find_h264_access_unit_delimiter(tsctx.video.data, tsctx.video.size);
                assert(0 == aud);
                assert(-1 == find_h264_access_unit_delimiter(tsctx.video.data + aud + 4, tsctx.video.size - aud - 4));

                // filter 0x09 AUD
                aud += 4 + h264_find_nalu(tsctx.video.data + aud + 4, tsctx.video.size - aud - 4);
                handler(param, tsctx.video.codecid, tsctx.video.pts, tsctx.video.dts, tsctx.video.data + aud, tsctx.video.size - aud);
            }
            else
            {
                //assert(0);
                memcpy(tsctx.video.data + tsctx.video.size, pes->pkt.data, pes->pkt.size);
                tsctx.video.size += pes->pkt.size;
                return 0;
            }
        }

        // copy new data
        tsctx.video.pts = pes->pts;
        tsctx.video.dts = pes->dts;
        tsctx.video.codecid = pes->codecid;
        tsctx.video.flags = pes->data_alignment_indicator ? 1 : 0;
        tsctx.video.size = pes->pkt.size - (p - pes->pkt.data);
        memcpy(tsctx.video.data, p, tsctx.video.size);
        assert(0 == find_h264_access_unit_delimiter(tsctx.video.data, tsctx.video.size)); // start with AUD
    }
    else if (PSI_STREAM_H265 == pes->codecid)
    {
        if (tsctx.video.size > 0)
        {
            int aud = find_h265_access_unit_delimiter(pes->pkt.data, pes->pkt.size);
            if (-1 != aud)
            {
                p += aud;

                // only one AUD
                assert(-1 == find_h265_access_unit_delimiter(pes->pkt.data + aud + 4, pes->pkt.size - aud - 4));

                // merge data
                memcpy(tsctx.video.data + tsctx.video.size, pes->pkt.data, aud);
                tsctx.video.size += aud;

                //assert(0 == pes->len || pes->payload.len == pes->len);
                aud = find_h265_access_unit_delimiter(tsctx.video.data, tsctx.video.size);
                assert(0 == aud);
                assert(-1 == find_h265_access_unit_delimiter(tsctx.video.data + aud + 4, tsctx.video.size - aud - 4));

                // filter AUD
                aud += 4 + h264_find_nalu(tsctx.video.data + aud + 4, tsctx.video.size - aud - 4);
                handler(param, tsctx.video.codecid, tsctx.video.pts, tsctx.video.dts, tsctx.video.data + aud, tsctx.video.size - aud);

                // copy new data
                tsctx.video.size = pes->pkt.size - aud;
                memcpy(tsctx.video.data, pes->pkt.data + aud, pes->pkt.size - aud);
            }
            else
            {
                //assert(0);
                memcpy(tsctx.video.data + tsctx.video.size, pes->pkt.data, pes->pkt.size);
                tsctx.video.size += pes->pkt.size;
                return 0;
            }
        }

        // copy new data
        tsctx.video.pts = pes->pts;
        tsctx.video.dts = pes->dts;
        tsctx.video.codecid = pes->codecid;
        tsctx.video.flags = pes->data_alignment_indicator ? 1 : 0;
        tsctx.video.size = pes->pkt.size - (p - pes->pkt.data);
        memcpy(tsctx.video.data, p, tsctx.video.size);
        assert(0 == find_h264_access_unit_delimiter(tsctx.video.data, tsctx.video.size)); // start with AUD
    }
    else
    {
        handler(param, pes->codecid, pes->pts, pes->dts, pes->pkt.data, pes->pkt.size);
    }
    return 0;
}

int mpeg_ts_packet_flush(onpacket handler, void* param)
{
    int aud, next;
    uint32_t j, k;
    for (j = 0; j < tsctx.pat.pmt_count; j++)
    {
        for (k = 0; k < tsctx.pat.pmts[j].stream_count; k++)
        {
            struct pes_t* pes = &tsctx.pat.pmts[j].streams[k];
            if (pes->pkt.size > 0)
            {
                mpeg_ts_packet_submit(pes, handler, param);
                pes->pkt.size = 0;
            }
        }
    }

    if (tsctx.video.size < 1)
        return 0;

    if (PSI_STREAM_H264 == tsctx.video.codecid)
    {
        aud = find_h264_access_unit_delimiter(tsctx.video.data, tsctx.video.size);
        assert(0 == aud);

        next = find_h264_access_unit_delimiter(tsctx.video.data + 4, tsctx.video.size - 4);
        assert(-1 == next);

        // filter 0x09 AUD
        aud += 4 + h264_find_nalu(tsctx.video.data + aud + 4, tsctx.video.size - aud - 4);
        handler(param, tsctx.video.codecid, tsctx.video.pts, tsctx.video.dts, tsctx.video.data + aud, tsctx.video.size - aud);
    }
    return 0;
}

int mpeg_ts_packet_dec(const uint8_t* data, size_t bytes, onpacket handler, void* param)
{
    uint32_t i, j, k;
	uint32_t PID;
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
            int64_t t;
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

			pat_read(&tsctx.pat, data + i, bytes - i);
		}
		else
		{
			for(j = 0; j < tsctx.pat.pmt_count; j++)
			{
				if(PID == tsctx.pat.pmts[j].pid)
				{
					if(TS_PAYLOAD_UNIT_START_INDICATOR(data))
						i += 1; // pointer 0x00

					pmt_read(&tsctx.pat.pmts[j], data + i, bytes - i);
					break;
				}
				else
				{
					for (k = 0; k < tsctx.pat.pmts[j].stream_count; k++)
					{
                        struct pes_t* pes = &tsctx.pat.pmts[j].streams[k];
						if (PID != pes->pid)
                            continue;

                        if (TS_PAYLOAD_UNIT_START_INDICATOR(data))
                        {
                            size_t n;

                            if (pes->pkt.size > 0)
                            {
                                mpeg_ts_packet_submit(pes, handler, param);
                                pes->pkt.size = 0;
                            }

                            n = pes_read_header(pes, data + i, bytes - i);
                            assert(n > 0);
                            i += n;
						}

                        if (!pes->pkt.data)
                            pes->pkt.data = (0xE0 == (pes->sid & 0xE0)) ? s_video : s_audio;

                        memcpy(pes->pkt.data + pes->pkt.size, data + i, bytes - i);
                        pes->pkt.size += bytes - i;

                        if (pes->pkt.size >= pes->len && pes->len > 0)
                        {
                            assert(pes->pkt.size == pes->len);
                            pes->pkt.size = pes->len; // why???
                            mpeg_ts_packet_submit(pes, handler, param);
                            pes->pkt.size = 0;
                        }

                        break; // find stream
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
