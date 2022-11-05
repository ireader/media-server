// ITU-T H.222.0(10/2014)
// Information technology - Generic coding of moving pictures and associated audio information: Systems
// 2.4.3.6 PES packet(p51)

#include "mpeg-pes-internal.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/// @return 0-error, other-pes header length
int pes_read_header(struct pes_t *pes, struct mpeg_bits_t* reader)
{
	uint8_t v8;
	uint16_t v16;
	uint32_t v32;
	size_t end;

	//pes->sid = mpeg_bits_read8(reader);
    pes->len = mpeg_bits_read16(reader);

	v8 = mpeg_bits_read8(reader);
    //assert(0x02 == ((v8 >> 6) & 0x3));
    pes->PES_scrambling_control = (v8 >> 4) & 0x3;
    pes->PES_priority = (v8 >> 3) & 0x1;
    pes->data_alignment_indicator = (v8 >> 2) & 0x1;
    pes->copyright = (v8 >> 1) & 0x1;
    pes->original_or_copy = v8 & 0x1;

	v8 = mpeg_bits_read8(reader);
    pes->PTS_DTS_flags = (v8 >> 6) & 0x3;
    pes->ESCR_flag = (v8 >> 5) & 0x1;
    pes->ES_rate_flag = (v8 >> 4) & 0x1;
    pes->DSM_trick_mode_flag = (v8 >> 3) & 0x1;
    pes->additional_copy_info_flag = (v8 >> 2) & 0x1;
    pes->PES_CRC_flag = (v8 >> 1) & 0x1;
    pes->PES_extension_flag = v8 & 0x1;

    pes->PES_header_data_length = mpeg_bits_read8(reader);
	if (pes->len > 0 && pes->len < pes->PES_header_data_length + 3)
		return MPEG_ERROR_INVALID_DATA; // skip invalid packet

	end = mpeg_bits_tell(reader) + pes->PES_header_data_length;
	if (mpeg_bits_error(reader) || end > mpeg_bits_length(reader))
		return MPEG_ERROR_NEED_MORE_DATA;
	
    if (0x02 & pes->PTS_DTS_flags)
    {
		v8 = mpeg_bits_read8(reader);
        //assert(0x20 == (v8 & 0x20));
        pes->pts = ((((uint64_t)v8 >> 1) & 0x07) << 30) | mpeg_bits_read30(reader);
    }
    //else
    //{
    //    pes->pts = PTS_NO_VALUE;
    //}

    if (0x01 & pes->PTS_DTS_flags)
    {
		v8 = mpeg_bits_read8(reader);
		//assert(0x10 == (v8 & 0x10));
		pes->dts = ((((uint64_t)v8 >> 1) & 0x07) << 30) | mpeg_bits_read30(reader);
    }
    else if(0x02 & pes->PTS_DTS_flags)
    {
        // has pts
        pes->dts = pes->pts;
    }
    //else
    //{
    //    pes->dts = PTS_NO_VALUE;
    //}

    if (pes->ESCR_flag)
    {
		v32 = mpeg_bits_read32(reader);
		v16 = mpeg_bits_read16(reader);
		pes->ESCR_base = (((uint64_t)((v32 >> 27) & 0x07)) << 30) | (((uint64_t)((v32 >> 11) & 0x7FFF)) << 15) | (((uint64_t)(v32 & 0x3FF)) << 5) | ((uint64_t)(v16 >> 11) & 0x1F);
		pes->ESCR_extension = (v16 >> 1) & 0x1FF;
    }

    if (pes->ES_rate_flag)
    {
		pes->ES_rate = (mpeg_bits_read8(reader) & 0x7F) << 15;
		pes->ES_rate |= mpeg_bits_read15(reader);
    }

    if (pes->DSM_trick_mode_flag)
    {
        // TODO:
		//mpeg_bits_skip(reader, 1);
    }

    if (pes->additional_copy_info_flag)
    {
		//mpeg_bits_skip(reader, 1);
    }

    if (pes->PES_CRC_flag)
    {
		//mpeg_bits_skip(reader, 2);
    }

    if (pes->PES_extension_flag)
    {
    }

	if (pes->len > 0)
	{
		if (pes->len < pes->PES_header_data_length + 3)
			return MPEG_ERROR_INVALID_DATA; // skip invalid packet
		pes->len -= pes->PES_header_data_length + 3;
	}
    
	assert(pes->len >= 0); // TS pes->len maybe 0(payload > 65535)
	mpeg_bits_seek(reader, end);
	assert(0 == mpeg_bits_error(reader));
	return mpeg_bits_error(reader) ? MPEG_ERROR_INVALID_DATA : MPEG_ERROR_OK;
}

/// @return 0-error, pes header length
size_t pes_write_header(const struct pes_t *pes, uint8_t* data, size_t bytes)
{
	uint8_t len = 0;
	uint8_t flags = 0x00;
	uint8_t *p = NULL;

	if (bytes < 9) return 0; // error

	// packet_start_code_prefix 0x000001
	data[0] = 0x00;
	data[1] = 0x00;
	data[2] = 0x01;
	data[3] = pes->sid;

	// skip PES_packet_length
	//data[4] = 0x00;
	//data[5] = 0x00;

	// '10'
	// PES_scrambling_control '00'
	// PES_priority '0'
	// data_alignment_indicator '1'
	// copyright '0'
	// original_or_copy '0'
	data[6] = 0x80;
	if(pes->data_alignment_indicator)
		data[6] |= 0x04;
	//if (IDR | subtitle | raw data)
		//data[6] |= 0x04;

	// PTS_DTS_flag 'xx'
	// ESCR_flag '0'
	// ES_rate_flag '0'
	// DSM_trick_mode_flag '0'
	// additional_copy_info_flag '0'
	// PES_CRC_flag '0'
	// PES_extension_flag '0'
	if(PTS_NO_VALUE != pes->pts)
	{
		flags |= 0x80;  // pts
		len += 5;
	}
	assert(PTS_NO_VALUE == pes->dts || pes->pts == pes->dts || PES_SID_VIDEO == data[3]); // audio PTS==DTS
	if(PTS_NO_VALUE != pes->dts /*&& PES_SID_VIDEO==(PES_SID_VIDEO&data[3])*/ && pes->dts != pes->pts)
	{
		flags |= 0x40;  // dts
		len += 5;
	}
	data[7] = flags;

	// PES_header_data_length : 8
	data[8] = len;

	if ((size_t)len + 9 > bytes)
		return 0; // error
	p = data + 9;

	if(flags & 0x80)
	{
		*p++ = ((flags >> 2) & 0x30)/* 0011/0010 */ | (((pes->pts >> 30) & 0x07) << 1) /* PTS 30-32 */ | 0x01 /* marker_bit */;
		*p++ = (pes->pts >> 22) & 0xFF; /* PTS 22-29 */
		*p++ = ((pes->pts >> 14) & 0xFE) /* PTS 15-21 */ | 0x01 /* marker_bit */;
		*p++ = (pes->pts >> 7) & 0xFF; /* PTS 7-14 */
		*p++ = ((pes->pts << 1) & 0xFE) /* PTS 0-6 */ | 0x01 /* marker_bit */;
	}

	if(flags & 0x40)
	{
		*p++ = 0x10 /* 0001 */ | (((pes->dts >> 30) & 0x07) << 1) /* DTS 30-32 */ | 0x01 /* marker_bit */;
		*p++ = (pes->dts >> 22) & 0xFF; /* DTS 22-29 */
		*p++ = ((pes->dts >> 14) & 0xFE) /* DTS 15-21 */ | 0x01 /* marker_bit */;
		*p++ = (pes->dts >> 7) & 0xFF; /* DTS 7-14 */
		*p++ = ((pes->dts << 1) & 0xFE) /* DTS 0-6 */ | 0x01 /* marker_bit */;
	}

	return p - data;
}

// ISO/IEC 11172-1 
// 2.4.3.3 Packet Layer (p20)
/*
packet() {
	packet_start_code_prefix							24 bslbf
	stream_id											8 uimsbf
	packet_length										16 uimsbf
	if (packet_start_code != private_stream_2) {
		while (nextbits() == '1')
			stuffing_byte								8 bslbf

		if (nextbits () == '01') {
			'01'										2 bslbf
			STD_buffer_scale							1 bslbf
			STD_buffer_size								13 uimsbf
		}
		if (nextbits() == '0010') {
			'0010'										4 bslbf
			presentation_time_stamp[32..30]				3 bslbf
			marker_bit									1 bslbf
			presentation_time_stamp[29..15]				15 bslbf
			marker_bit									1 bslbf
			presentation_time_stamp[14..0]				15 bslbf
			marker_bit									1 bslbf
		}
			else if (nextbits() == '0011') {
			'0011'										4 bslbf
			presentation_time_stamp[32..30]				3 bslbf
			marker_bit									1 bslbf
			presentation_time_stamp[29..15]				15 bslbf
			marker_bit									1 bslbf
			presentation_time_stamp[14..0]				15 bslbf
			marker_bit									1 bslbf
			'0001'										4 bslbf
			decoding_time_stamp[32..30]					3 bslbf
			marker_bit									1 bslbf
			decoding_time_stamp[29..15]					15 bslbf
			marker_bit									1 bslbf
			decoding_time_stamp[14..0]					15 bslbf
			marker_bit									1 bslbf
		}
		else
			'0000 1111'									8 bslbf
	}

	for (i = 0; i < N; i++) {
		packet_data_byte								8 bslbf
	}
}
*/
int pes_read_mpeg1_header(struct pes_t *pes, struct mpeg_bits_t* reader)
{
	uint8_t v8;
	size_t offset;

	//pes->sid = mpeg_bits_read8(reader);
	pes->len = mpeg_bits_read16(reader);
	offset = mpeg_bits_tell(reader);

	do
	{
		v8 = mpeg_bits_read8(reader);
	} while (0 == mpeg_bits_error(reader) && v8 == 0xFF);
	
	if (0x40 == (0xC0 & v8))
	{
		mpeg_bits_skip(reader, 2); // skip STD_buffer_scale / STD_buffer_size
		v8 = mpeg_bits_read8(reader);
	}

	if (0x20 == (0xF0 & v8))
	{
		pes->pts = ((((uint64_t)v8 >> 1) & 0x07) << 30) | mpeg_bits_read30(reader);
	}
	else if (0x30 == (0xF0 & v8))
	{
		pes->pts = ((((uint64_t)v8 >> 1) & 0x07) << 30) | mpeg_bits_read30(reader);

		v8 = mpeg_bits_read8(reader);
		pes->dts = ((((uint64_t)v8 >> 1) & 0x07) << 30) | mpeg_bits_read30(reader);
	}
	else
	{
		assert(0x0F == v8);
	}

	if (mpeg_bits_error(reader))
		return MPEG_ERROR_NEED_MORE_DATA;

	offset = mpeg_bits_tell(reader) - offset;
	if (pes->len > 0)
	{
		if (pes->len < offset)
			return MPEG_ERROR_INVALID_DATA; // invalid data length
		pes->len -= (uint32_t)offset;
	}

	assert(0 == mpeg_bits_error(reader));
	return MPEG_ERROR_OK;
}

uint16_t mpeg_bits_read15(struct mpeg_bits_t* reader)
{
	uint16_t v;
	v = ((uint16_t)mpeg_bits_read8(reader)) << 7;
	v |= (mpeg_bits_read8(reader) >> 1) & 0x7F;
	return v;
}

uint32_t mpeg_bits_read30(struct mpeg_bits_t* reader)
{
	uint32_t v;
	v = ((uint32_t)mpeg_bits_read15(reader)) << 15;
	v |= mpeg_bits_read15(reader);
	return v;
}

uint64_t mpeg_bits_read45(struct mpeg_bits_t* reader)
{
	uint64_t v;
	v = ((uint64_t)mpeg_bits_read15(reader)) << 30;
	v |= ((uint64_t)mpeg_bits_read15(reader)) << 15;
	v |= mpeg_bits_read15(reader);
	return v;
}
