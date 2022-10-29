// ITU-T H.222.0(10/2014)
// Information technology - Generic coding of moving pictures and associated audio information: Systems
// 2.5.5 Program stream directory(p84)

#include "mpeg-ps-internal.h"
#include <assert.h>

#if 1
int psd_read(struct psd_t* psd, struct mpeg_bits_t* reader)
{
	size_t i, end;
	size_t packet_length;
	uint8_t v8;
	uint64_t v64;
	uint16_t number_of_access_units;

	// Table 2-42 - Program stream directory packet(p81)
	packet_length = mpeg_bits_read16(reader);
	end = mpeg_bits_tell(reader) + packet_length;
	if (mpeg_bits_error(reader) || end > mpeg_bits_length(reader))
		return MPEG_ERROR_NEED_MORE_DATA;

	//assert((0x01 & data[7]) == 0x01); // 'xxxxxxx1'
	number_of_access_units = mpeg_bits_read15(reader); // (data[6] << 7) | ((data[7] >> 1) & 0x7F);
	assert(number_of_access_units <= N_ACCESS_UNIT);

	//assert((0x01 & data[9]) == 0x01); // 'xxxxxxx1'
	//assert((0x01 & data[11]) == 0x01); // 'xxxxxxx1'
	//assert((0x01 & data[13]) == 0x01); // 'xxxxxxx1'
	psd->prev_directory_offset = mpeg_bits_read45(reader);
	//assert((0x01 & data[15]) == 0x01); // 'xxxxxxx1'
	//assert((0x01 & data[17]) == 0x01); // 'xxxxxxx1'
	//assert((0x01 & data[19]) == 0x01); // 'xxxxxxx1'
	psd->next_directory_offset = mpeg_bits_read45(reader);

	// access unit
	for (i = 0; 0 == mpeg_bits_error(reader) && mpeg_bits_tell(reader) + 18 <= end && i < number_of_access_units && i < N_ACCESS_UNIT; i++)
	{
		psd->units[i].packet_stream_id = mpeg_bits_read8(reader);
		//assert((0x01 & data[2]) == 0x01); // 'xxxxxxx1'
		//assert((0x01 & data[4]) == 0x01); // 'xxxxxxx1'
		//assert((0x01 & data[6]) == 0x01); // 'xxxxxxx1'
		v64 = mpeg_bits_read45(reader);
		psd->units[i].pes_header_position_offset = v64 & 0x3FFFFF;
		psd->units[i].pes_header_position_offset_sign = (psd->units[i].pes_header_position_offset & 0x400000) ? 1 : 0;
		psd->units[i].reference_offset = mpeg_bits_read16(reader);

		//assert((0x81 & data[9]) == 0x81); // '1xxxxxx1'
		v8 = mpeg_bits_read8(reader);
		if (psd->units[i].packet_stream_id == 0xFD)
		{
			psd->units[i].packet_stream_id_extension_msbs = (v8 >> 4) & 0x07;
		}
		else
		{
			assert((0x70 & v8) == 0x00); // '1000xxx1'
		}

		//assert((0x01 & data[11]) == 0x01); // 'xxxxxxx1'
		//assert((0x01 & data[13]) == 0x01); // 'xxxxxxx1'
		psd->units[i].PTS = (((v8 >> 1) & 0x07) << 30) | mpeg_bits_read30(reader);

		//assert((0x01 & data[15]) == 0x01); // 'xxxxxxx1'
		psd->units[i].bytes_to_read = mpeg_bits_read15(reader) << 15;
		psd->units[i].bytes_to_read |= mpeg_bits_read8(reader);

		v8 = mpeg_bits_read8(reader);
		//assert((0x80 & data[17]) == 0x80); // '1xxxxxxx'
		psd->units[i].intra_coded_indicator = (v8 >> 6) & 0x01;
		psd->units[i].coding_parameters_indicator = (v8 >> 4) & 0x03;
		if (0xFD == psd->units[i].packet_stream_id)
		{
			psd->units[i].packet_stream_id_extension_lsbs = v8 & 0x0F;
		}
		else
		{
			assert((0x0F & v8) == 0x00); // '1xxx0000'
		}
	}

	assert(0 == mpeg_bits_error(reader));
	assert(end == mpeg_bits_tell(reader));
	return MPEG_ERROR_OK;
}
#else
size_t psd_read(struct psd_t *psd, const uint8_t* data, size_t bytes)
{
	size_t i, j;
	size_t packet_length;
	uint16_t number_of_access_units;

	// Table 2-42 - Program stream directory packet(p81)
	assert(0x00==data[0] && 0x00==data[1] && 0x01==data[2] && 0xFF==data[3]);
	packet_length = (((uint16_t)data[4]) << 8) | data[5];
	assert(bytes >= (size_t)packet_length + 6);
	if (bytes < 20 || packet_length < 20 - 6)
		return 0; // invalid data length

	assert((0x01 & data[7]) == 0x01); // 'xxxxxxx1'
	number_of_access_units = (data[6] << 7) | ((data[7] >> 1) & 0x7F);
	assert(number_of_access_units <= N_ACCESS_UNIT);

	assert((0x01 & data[9]) == 0x01); // 'xxxxxxx1'
	assert((0x01 & data[11]) == 0x01); // 'xxxxxxx1'
	assert((0x01 & data[13]) == 0x01); // 'xxxxxxx1'
	psd->prev_directory_offset = (uint64_t)(((uint64_t)data[8] << 38) | ((((uint64_t)data[9] >> 1) & 0x7F) << 30) | ((uint64_t)data[10] << 22) | ((((uint64_t)data[11] >> 1) & 0x7F) << 15) | ((uint64_t)data[12] << 7) | (((uint64_t)data[13] >> 1) & 0x7F));
	assert((0x01 & data[15]) == 0x01); // 'xxxxxxx1'
	assert((0x01 & data[17]) == 0x01); // 'xxxxxxx1'
	assert((0x01 & data[19]) == 0x01); // 'xxxxxxx1'
	psd->next_directory_offset = (uint64_t)(((uint64_t)data[14] << 38) | ((((uint64_t)data[15] >> 1) & 0x7F) << 30) | ((uint64_t)data[16] << 22) | ((((uint64_t)data[17] >> 1) & 0x7F) << 15) | ((uint64_t)data[18] << 7) | (((uint64_t)data[19] >> 1) & 0x7F));

	// access unit
	j = 20;
	for(i = 0; j + 18 <= packet_length + 6 && i < number_of_access_units && i < N_ACCESS_UNIT; i++)
	{
		psd->units[i].packet_stream_id = data[j];
		psd->units[i].pes_header_position_offset_sign = (data[j+1] >> 7) & 0x01;
		assert((0x01 & data[j+2]) == 0x01); // 'xxxxxxx1'
		assert((0x01 & data[j+4]) == 0x01); // 'xxxxxxx1'
		assert((0x01 & data[j+6]) == 0x01); // 'xxxxxxx1'
		psd->units[i].pes_header_position_offset = (uint64_t)(((uint64_t)(data[j+1] & 0x7F) << 37) | ((((uint64_t)data[j+2] >> 1) & 0x7F) << 30) | ((uint64_t)data[j+3] << 22) | ((((uint64_t)data[j+4] >> 1) & 0x7F) << 15) | ((uint64_t)data[j+5] << 7) | (((uint64_t)data[j+6] >> 1) & 0x7F));
		psd->units[i].reference_offset = (data[j+7] << 8) | data[j+8];

		assert((0x81 & data[j+9]) == 0x81); // '1xxxxxx1'
		if(psd->units[i].packet_stream_id == 0xFD)
		{
			psd->units[i].packet_stream_id_extension_msbs = (data[j+9] >> 4) & 0x07;
		}
		else
		{
			assert((0x70 & data[j+9]) == 0x00); // '1000xxx1'
		}

		assert((0x01 & data[j+11]) == 0x01); // 'xxxxxxx1'
		assert((0x01 & data[j+13]) == 0x01); // 'xxxxxxx1'
		psd->units[i].PTS = (uint64_t)(((uint64_t)((data[j+9] >> 1) & 0x07) << 30) | ((uint64_t)data[j+10] << 22) | ((((uint64_t)data[j+11] >> 1) & 0x7F) << 15) | ((uint64_t)data[j+12] << 7) | (((uint64_t)data[j+13] >> 1) & 0x7F));

		assert((0x01 & data[j+15]) == 0x01); // 'xxxxxxx1'
		psd->units[i].bytes_to_read = (uint32_t)( ((uint32_t)data[j+14] << 15) | (((data[j+15] >> 1) & 0x7F) << 8) | data[j+16]);

		assert((0x80 & data[j+17]) == 0x80); // '1xxxxxxx'
		psd->units[i].intra_coded_indicator = (data[j+17] >> 6) & 0x01;
		psd->units[i].coding_parameters_indicator = (data[j+17] >> 4) & 0x03;
		if(0xFD == psd->units[i].packet_stream_id)
		{
			psd->units[i].packet_stream_id_extension_lsbs = data[j+17] & 0x0F;
		}
		else
		{
			assert((0x0F & data[j+17]) == 0x00); // '1xxx0000'
		}

		j += 18;
	}

	return j;
}
#endif