#include "mpeg-ps-internal.h"
#include <assert.h>

// 2.5.3.3 Pack layer of program stream (p78)
// Table 2-38 - Program stream pack
// Table 2-39 - Program stream pack header
#if 1
int pack_header_read(struct ps_pack_header_t* h, struct mpeg_bits_t* reader)
{
	uint8_t v8;
	uint64_t v64;
	uint8_t stuffing_length;
	size_t header_length;

	v8 = mpeg_bits_read8(reader); // data[4]
	if (0 == (0xC0 & v8))
	{
		// MPEG-1
		// ISO/IEC 11172-1
		/*
		pack() {
			pack_start_code						32 bslbf
			'0010'								4 bslbf
			system_clock_reference [32..30]		3 bslbf
			marker_bit							1 bslbf
			system_clock_reference [29..15]		15 bslbf
			marker_bit							1 bslbf
			system_clock_reference [14..0]		15 bslbf
			marker_bit							1 bslbf
			marker_bit							1 bslbf
			mux_rate							22 uimsbf
			marker_bit							1 bslbf
			if (nextbits() == system_header_start_code)
				system_header ()
			while (nextbits() == packet_start_code)
				packet()
		}
		*/
		h->mpeg2 = 0;
		//assert(0x20 == (0xF0 & v8));
		h->system_clock_reference_base = (((uint64_t)(v8 >> 1) & 0x07) << 30) | mpeg_bits_read30(reader);
		h->system_clock_reference_extension = 1;
		h->program_mux_rate = (mpeg_bits_read8(reader) & 0x7F) << 15;
		h->program_mux_rate |= mpeg_bits_read15(reader);
		return mpeg_bits_error(reader) ? MPEG_ERROR_NEED_MORE_DATA : MPEG_ERROR_OK;
	}
	else
	{
		h->mpeg2 = 1;
		v64 = mpeg_bits_read64(reader); // data[5-12]
		//assert((0x44 & data[4]) == 0x44); // '01xxx1xx'
		//assert((0x04 & data[6]) == 0x04); // 'xxxxx1xx'
		//assert((0x04 & data[8]) == 0x04); // 'xxxxx1xx'
		//assert((0x01 & data[9]) == 0x01); // 'xxxxxxx1'
		h->system_clock_reference_base = (((v8 >> 3) & 0x07) << 30) | ((v8 & 0x03) << 28) | (((v64 >> 51) & 0x1FFF) << 15) | ((v64 >> 35) & 0x7FFF);
		h->system_clock_reference_extension = (uint32_t)((v64 >> 25) & 0x1F);

		//assert((0x03 & v64) == 0x03); // 'xxxxxx11'
		h->program_mux_rate = (uint32_t)((v64 >> 2) & 0x3FFFFF);

		//assert((0xF8 & data[13]) == 0x00); // '00000xxx'
		stuffing_length = mpeg_bits_read8(reader) & 0x07; // stuffing

		header_length = 14 + stuffing_length;
		mpeg_bits_skip(reader, stuffing_length);
		return mpeg_bits_error(reader) ? MPEG_ERROR_NEED_MORE_DATA : MPEG_ERROR_OK;
	}
}

#else
size_t pack_header_read(struct ps_pack_header_t *h, const uint8_t* data, size_t bytes)
{
    uint8_t stuffing_length;
	size_t header_length;

    if (bytes < 14) return 0;
    assert(0x00 == data[0] && 0x00 == data[1] && 0x01 == data[2] && PES_SID_START == data[3]);
	if (0 == (0xC0 & data[4]))
	{
		// MPEG-1
		h->mpeg2 = 0;
		assert(0x20 == (0xF0 & data[4]));
		h->system_clock_reference_base = (((uint64_t)(data[4] >> 1) & 0x07) << 30) | ((uint64_t)data[5] << 22) | (((uint64_t)data[6] >> 1) << 15) | ((uint64_t)data[7] << 7) | (data[8] >> 1);
		h->system_clock_reference_extension = 1;
		h->program_mux_rate = ((data[9] >> 1) << 15) | (data[10] << 7) | (data[11] >> 1);
		return 12;
	}
	else
	{
		h->mpeg2 = 1;
		assert((0x44 & data[4]) == 0x44); // '01xxx1xx'
		assert((0x04 & data[6]) == 0x04); // 'xxxxx1xx'
		assert((0x04 & data[8]) == 0x04); // 'xxxxx1xx'
		assert((0x01 & data[9]) == 0x01); // 'xxxxxxx1'
		h->system_clock_reference_base = (((uint64_t)(data[4] >> 3) & 0x07) << 30) | (((uint64_t)data[4] & 0x3) << 28) | ((uint64_t)data[5] << 20) | ((((uint64_t)data[6] >> 3) & 0x1F) << 15) | (((uint64_t)data[6] & 0x3) << 13) | ((uint64_t)data[7] << 5) | ((data[8] >> 3) & 0x1F);
		h->system_clock_reference_extension = ((data[8] & 0x3) << 7) | ((data[9] >> 1) & 0x7F);

		assert((0x03 & data[12]) == 0x03); // 'xxxxxx11'
		h->program_mux_rate = (data[10] << 14) | (data[11] << 6) | ((data[12] >> 2) & 0x3F);

		//assert((0xF8 & data[13]) == 0x00); // '00000xxx'
		stuffing_length = data[13] & 0x07; // stuffing

		header_length = 14 + stuffing_length;
		if (header_length > bytes)
			return 0;
		return header_length;
	}
}
#endif

size_t pack_header_write(const struct ps_pack_header_t *h, uint8_t *data)
{
    // pack_start_code
    nbo_w32(data, 0x000001BA);

    // 33-system_clock_reference_base + 9-system_clock_reference_extension
    // '01xxx1xx xxxxxxxx xxxxx1xx xxxxxxxx xxxxx1xx xxxxxxx1'
    data[4] = 0x44 | (((h->system_clock_reference_base >> 30) & 0x07) << 3) | ((h->system_clock_reference_base >> 28) & 0x03);
    data[5] = ((h->system_clock_reference_base >> 20) & 0xFF);
    data[6] = 0x04 | (((h->system_clock_reference_base >> 15) & 0x1F) << 3) | ((h->system_clock_reference_base >> 13) & 0x03);
    data[7] = ((h->system_clock_reference_base >> 5) & 0xFF);
    data[8] = 0x04 | ((h->system_clock_reference_base & 0x1F) << 3) | ((h->system_clock_reference_extension >> 7) & 0x03);
    data[9] = 0x01 | ((h->system_clock_reference_extension & 0x7F) << 1);

    // program_mux_rate
    // 'xxxxxxxx xxxxxxxx xxxxxx11'
    data[10] = (uint8_t)(h->program_mux_rate >> 14);
    data[11] = (uint8_t)(h->program_mux_rate >> 6);
    data[12] = (uint8_t)(0x03 | ((h->program_mux_rate & 0x3F) << 2));

    // stuffing length
    // '00000xxx'
    data[13] = 0xF8;

    return 14;
}
