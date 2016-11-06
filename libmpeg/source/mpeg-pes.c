// ITU-T H.222.0(10/2014)
// Information technology ¨C Generic coding of moving pictures and associated audio information: Systems
// 2.4.3.6 PES packet(p51)

#include <stdio.h>
#include "mpeg-ps-proto.h"
#include "mpeg-ts-proto.h"
#include "mpeg-pes-proto.h"
#include <memory.h>
#include <assert.h>

static int pes_payload(void* param, const uint8_t* data, size_t bytes)
{
	pes_t *pes;
	pes = (pes_t*)param;

	memcpy(pes->payload, data, bytes);
	pes->payload_len = bytes;
	return 0;
}

static size_t pes_packet(const uint8_t* data, size_t bytes, pes_t *pes)
{
	size_t i;

	assert(0x00==data[0] && 0x00==data[1] && 0x01==data[2]);
	pes->sid = data[3];
	pes->len = (data[4] << 8) | data[5];

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

	i++;
	if(0x02 & pes->PTS_DTS_flags)
	{
		assert(0x20 == (data[i] & 0x20));
		pes->pts = ((((uint64_t)data[i] >> 1) & 0x07) << 30) | ((uint64_t)data[i+1] << 22) | ((((uint64_t)data[i+2] >> 1) & 0x7F) << 15) | ((uint64_t)data[i+3] << 7) | ((data[i+4] >> 1) & 0x7F);

		i += 5;
	}
	else
	{
		pes->pts = PTS_NO_VALUE;
	}

	if(0x01 & pes->PTS_DTS_flags)
	{
		assert(0x10 == (data[i] & 0x10));
		pes->dts = ((((uint64_t)data[i] >> 1) & 0x07) << 30) | ((uint64_t)data[i+1] << 22) | ((((uint64_t)data[i+2] >> 1) & 0x7F) << 15) | ((uint64_t)data[i+3] << 7) | ((data[i+4] >> 1) & 0x7F);
		i += 5;
	}
	else
	{
		pes->dts = PTS_NO_VALUE;
	}

	if(pes->ESCR_flag)
	{
		pes->ESCR_base = ((((uint64_t)data[i] >> 3) & 0x07) << 30) | (((uint64_t)data[i] & 0x03) << 28) | ((uint64_t)data[i+1] << 20) | ((((uint64_t)data[i+2] >> 3) & 0x1F) << 15) | (((uint64_t)data[i+2] & 0x3) << 13) | ((uint64_t)data[i+3] << 5) | ((data[i+4] >> 3) & 0x1F);
		pes->ESCR_extension = ((data[i+4] & 0x03) << 7) | ((data[i+5] >> 1) & 0x7F);
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
	// offset = 32bits-start_code + 16bits-PES_packet_length + 24bits-PES_xxx_flag + PES_header_data_length
	i = 4 + 2 + 3 + pes->PES_header_data_length;
	pes_payload(pes, data + i, bytes - i);

	return 0;
}

size_t pes_read(const uint8_t* data, size_t bytes, psm_t *psm, pes_t *pes)
{
	psd_t psd;

	assert(0x00==data[0] && 0x00==data[1] && 0x01==data[2]);

	// stream id
	switch(data[3])
	{
	case PES_SID_PSM:
		psm_read(data, bytes, psm);
		break;

	case PES_SID_PSD:
		psd_read(data, bytes, &psd);
		break;

	case PES_SID_PRIVATE_2:
	case PES_SID_ECM:
	case PES_SID_EMM:	
	case PES_SID_DSMCC:
	case PES_SID_H222_E:
		// stream data
		break;

	case PES_SID_PADDING:
		// padding
		break;

	default:
		assert(PES_SID_END != data[3]);
		pes_packet(data, bytes, pes);
	}

	return 0;
}

size_t pes_write_header(int64_t pts, int64_t dts, int streamId, uint8_t* data)
{
	uint8_t len = 0;
	uint8_t flags = 0x00;
	uint8_t *p = NULL;

	// packet_start_code_prefix 0x000001
	data[0] = 0x00;
	data[1] = 0x00;
	data[2] = 0x01;
	data[3] = (uint8_t)streamId;

	// skip PES_packet_length
	//data[4] = 0x00;
	//data[5] = 0x00;

	// '10'
	// PES_scrambling_control '00'
	// PES_priority '0'
	// data_alignment_indicator '1'
	// copyright '0'
	// original_or_copy '0'
	//data[6] = SUBTITLE ? 0x84 : 0x80;
	data[6] = 0x80;
	//if (IDR | subtitle | raw data)
		//data[6] |= 0x04;

	// PTS_DTS_flag 'xx'
	// ESCR_flag '0'
	// ES_rate_flag '0'
	// DSM_trick_mode_flag '0'
	// additional_copy_info_flag '0'
	// PES_CRC_flag '0'
	// PES_extension_flag '0'
	if(PTS_NO_VALUE != pts)
	{
		flags |= 0x80;  // pts
		len += 5;
	}
	assert(PTS_NO_VALUE == dts || pts == dts || PES_SID_VIDEO == data[3]); // audio PTS==DTS
	if(PTS_NO_VALUE != dts && /*PES_SID_VIDEO==data[3] && */dts != pts)
	{
		flags |= 0x40;  // dts
		len += 5;
	}
	data[7] = flags;

	// PES_header_data_length : 8
	data[8] = len;

	p = data + 9;
	if(flags & 0x80)
	{
		*p++ = ((flags >> 2) & 0x30)/* 0011/0010 */ | (((pts >> 30) & 0x07) << 1) /* PTS 30-32 */ | 0x01 /* marker_bit */;
		*p++ = (pts >> 22) & 0xFF; /* PTS 22-29 */
		*p++ = ((pts >> 14) & 0xFE) /* PTS 15-21 */ | 0x01 /* marker_bit */;
		*p++ = (pts >> 7) & 0xFF; /* PTS 7-14 */
		*p++ = ((pts << 1) & 0xFE) /* PTS 0-6 */ | 0x01 /* marker_bit */;
	}

	if(flags & 0x40)
	{
		*p++ = 0x10 /* 0001 */ | (((dts >> 30) & 0x07) << 1) /* DTS 30-32 */ | 0x01 /* marker_bit */;
		*p++ = (dts >> 22) & 0xFF; /* DTS 22-29 */
		*p++ = ((dts >> 14) & 0xFE) /* DTS 15-21 */ | 0x01 /* marker_bit */;
		*p++ = (dts >> 7) & 0xFF; /* DTS 7-14 */
		*p++ = ((dts << 1) & 0xFE) /* DTS 0-6 */ | 0x01 /* marker_bit */;
	}

	return p - data;
}
