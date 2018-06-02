// ITU-T H.222.0(10/2014)
// Information technology ¨C Generic coding of moving pictures and associated audio information: Systems
// 2.5.5 Program stream directory(p84)

#include "mpeg-ps-proto.h"
#include <assert.h>

size_t psd_read(struct psd_t *psd, const uint8_t* data, size_t bytes)
{
	int i, j;
	uint16_t packet_length;
	uint16_t number_of_access_units;

	// Table 2-42 ¨C Program stream directory packet(p81)
	assert(0x00==data[0] && 0x00==data[1] && 0x01==data[2] && 0xFF==data[3]);
	packet_length = (((uint16_t)data[4]) << 8) | data[5];
	assert(bytes >= (size_t)packet_length + 6);

	assert((0x01 & data[7]) == 0x01); // 'xxxxxxx1'
	number_of_access_units = (data[6] << 8) | ((data[7] >> 7) & 0x7F);
	assert(number_of_access_units <= N_ACCESS_UNIT);

	assert((0x01 & data[9]) == 0x01); // 'xxxxxxx1'
	assert((0x01 & data[11]) == 0x01); // 'xxxxxxx1'
	assert((0x01 & data[13]) == 0x01); // 'xxxxxxx1'
	psd->prev_directory_offset = (uint64_t)(((uint64_t)data[8] << 38) | ((((uint64_t)data[9] >> 7) & 0x7F) << 30) | ((uint64_t)data[10] << 22) | ((((uint64_t)data[11] >> 7) & 0x7F) << 15) | ((uint64_t)data[12] << 7) | (((uint64_t)data[13] >> 7) & 0x7F));
	assert((0x01 & data[15]) == 0x01); // 'xxxxxxx1'
	assert((0x01 & data[17]) == 0x01); // 'xxxxxxx1'
	assert((0x01 & data[19]) == 0x01); // 'xxxxxxx1'
	psd->next_directory_offset = (uint64_t)(((uint64_t)data[14] << 38) | ((((uint64_t)data[15] >> 7) & 0x7F) << 30) | ((uint64_t)data[16] << 22) | ((((uint64_t)data[17] >> 7) & 0x7F) << 15) | ((uint64_t)data[18] << 7) | (((uint64_t)data[19] >> 7) & 0x7F));

	// access unit
	j = 20;
	for(i = 0; i < number_of_access_units; i++)
	{
		psd->units[i].packet_stream_id = data[j];
		psd->units[i].pes_header_position_offset_sign = (data[j+1] >> 7) & 0x01;
		assert((0x01 & data[j+2]) == 0x01); // 'xxxxxxx1'
		assert((0x01 & data[j+4]) == 0x01); // 'xxxxxxx1'
		assert((0x01 & data[j+6]) == 0x01); // 'xxxxxxx1'
		psd->units[i].pes_header_position_offset = (uint64_t)(((uint64_t)(data[j+1] & 0x7F) << 38) | ((((uint64_t)data[j+2] >> 7) & 0x7F) << 30) | ((uint64_t)data[j+3] << 22) | ((((uint64_t)data[j+4] >> 7) & 0x7F) << 15) | ((uint64_t)data[j+5] << 7) | (((uint64_t)data[j+6] >> 7) & 0x7F));
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
		psd->units[i].PTS = (uint64_t)(((uint64_t)((data[j+9] >> 1) & 0x07) << 30) | ((uint64_t)data[j+10] << 22) | ((((uint64_t)data[j+11] >> 7) & 0x7F) << 15) | ((uint64_t)data[j+12] << 7) | (((uint64_t)data[j+13] >> 7) & 0x7F));

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

	return j+1;
}
