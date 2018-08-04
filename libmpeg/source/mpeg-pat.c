// ITU-T H.222.0(10/2014)
// Information technology ¨C Generic coding of moving pictures and associated audio information: Systems
// 2.4.4.3 Program association table(p65)

#include "mpeg-ts-proto.h"
#include "mpeg-util.h"
#include <assert.h>

size_t pat_read(struct pat_t *pat, const uint8_t* data, size_t bytes)
{
	// Table 2-30 ¨C Program association section(p65)

	uint32_t i = 0;
	uint32_t j = 0;
//	uint32_t crc = 0;

	uint32_t table_id = data[0];
	uint32_t section_syntax_indicator = (data[1] >> 7) & 0x01;
//	uint32_t zero = (data[1] >> 6) & 0x01;
//	uint32_t reserved = (data[1] >> 4) & 0x03;
	uint32_t section_length = ((data[1] & 0x0F) << 8) | data[2];
	uint32_t transport_stream_id = (data[3] << 8) | data[4];
//	uint32_t reserved2 = (data[5] >> 6) & 0x03;
	uint32_t version_number = (data[5] >> 1) & 0x1F;
//	uint32_t current_next_indicator = data[5] & 0x01;
//	uint32_t sector_number = data[6];
//	uint32_t last_sector_number = data[7];

//	printf("PAT: %0x %0x %0x %0x %0x %0x %0x %0x\n", (unsigned int)data[0], (unsigned int)data[1], (unsigned int)data[2], (unsigned int)data[3], (unsigned int)data[4], (unsigned int)data[5], (unsigned int)data[6], (unsigned int)data[7]);

	assert(PAT_TID_PAS == table_id);
	assert(1 == section_syntax_indicator);
	pat->tsid = transport_stream_id;
	pat->ver = version_number;

    // TODO: version_number change, reload pmts
	assert(bytes >= section_length + 3); // PAT = section_length + 3
	for(i = 8; i < section_length + 8 - 5 - 4 && j < sizeof(pat->pmts)/sizeof(pat->pmts[0]); i += 4) // 4:CRC, 5:follow section_length item
	{
		pat->pmts[j].pn = (data[i] << 8) | data[i+1];
		pat->pmts[j].pid = ((data[i+2] & 0x1F) << 8) | data[i+3];
//		printf("PAT[%d]: pn: %0x, pid: %0x\n", j, pat->pmt[j].pn, pat->pmt[j].pid);
		j++;
	}

	pat->pmt_count = j;

	//assert(i+4 == bytes);
	//crc = (data[i] << 24) | (data[i+1] << 16) | (data[i+2] << 8) | data[i+3];
	//crc = mpeg_crc32(-1, data, bytes-4);
	assert(0 == mpeg_crc32(0xffffffff, data, section_length+3));
	return 0;
}

size_t pat_write(const struct pat_t *pat, uint8_t *data)
{
	// Table 2-30 ¨C Program association section(p65)

	uint32_t i = 0;
	uint32_t len = 0;
	uint32_t crc = 0;

	len = pat->pmt_count * 4 + 5 + 4; // 5 bytes remain header and 4 bytes crc32

	// shall not exceed 1021 (0x3FD).
	assert(len <= 1021);
	assert(len <= TS_PACKET_SIZE - 7);

	data[0] = PAT_TID_PAS;	// program association table

	// section_syntax_indicator = '1'
	// '0'
	// reserved '11'
	nbo_w16(data + 1, (uint16_t)(0xb000 | len));

	// transport_stream_id
	nbo_w16(data + 3, (uint16_t)pat->tsid);

	// reserved '11'
	// version_number 'xxxxx'
	// current_next_indicator '1'
	data[5] = (uint8_t)(0xC1 | (pat->ver << 1));

	// section_number/last_section_number
	data[6] = 0x00;
	data[7] = 0x00;

	for(i = 0; i < pat->pmt_count; i++)
	{
		nbo_w16(data + 8 + i * 4 + 0, (uint16_t)pat->pmts[i].pn);
		nbo_w16(data + 8 + i * 4 + 2, (uint16_t)(0xE000 | pat->pmts[i].pid));
	}

	// crc32
	crc = mpeg_crc32(0xffffffff, data, len-1);
	//put32(data + section_length - 1, crc);
	data[len - 1 + 3] = (crc >> 24) & 0xFF;
	data[len - 1 + 2] = (crc >> 16) & 0xFF;
	data[len - 1 + 1] = (crc >> 8) & 0xFF;
	data[len - 1 + 0] = crc & 0xFF;

	return len + 3; // total length
}
