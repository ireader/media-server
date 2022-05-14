// ITU-T H.222.0(10/2014)
// Information technology - Generic coding of moving pictures and associated audio information: Systems
// 2.4.4.3 Program association table(p65)

#include "mpeg-ts-proto.h"
#include "mpeg-util.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct pmt_t* pat_alloc_pmt(struct pat_t* pat)
{
	void* ptr;
	unsigned int n;

	if (NULL == pat->pmts)
	{
		assert(0 == pat->pmt_count);
		assert(0 == pat->pmt_capacity);
		pat->pmts = pat->pmt_default;
		pat->pmt_capacity = sizeof(pat->pmt_default) / sizeof(pat->pmt_default[0]);
	}

	if (pat->pmt_count >= pat->pmt_capacity)
	{
		if (pat->pmt_count + 1 > 65535)
		{
			assert(0);
			return NULL;
		}

		n = pat->pmt_capacity + pat->pmt_capacity / 4 + 4;
		ptr = realloc(pat->pmts == pat->pmt_default ? NULL : pat->pmts, sizeof(pat->pmts[0]) * n);
		if (!ptr)
			return NULL;

		if (pat->pmts == pat->pmt_default)
			memmove(ptr, pat->pmt_default, sizeof(pat->pmt_default));
		pat->pmts = (struct pmt_t*)ptr;
		pat->pmt_capacity = n;
	}

	// new pmt
	memset(&pat->pmts[pat->pmt_count], 0, sizeof(pat->pmts[0]));
	return &pat->pmts[pat->pmt_count];
}

static struct pmt_t* pat_fetch(struct pat_t* pat, uint16_t pid)
{
	unsigned int i;
	struct pmt_t* pmt;
	for(i = 0; i < pat->pmt_count; i++)
    {
        if(pat->pmts[i].pid == pid)
            return &pat->pmts[i];
    }
	
    // new pmt
	pmt = pat_alloc_pmt(pat);
	pat->pmt_count++;
	return pmt;
}

size_t pat_read(struct pat_t *pat, const uint8_t* data, size_t bytes)
{
	// Table 2-30 Program association section(p65)

    struct pmt_t* pmt;
	uint32_t i = 0;
    uint16_t pn, pid;
//	uint32_t crc = 0;
	uint32_t section_length, transport_stream_id, version_number;

	if(bytes < 8)
		return 0; // invalid data length
//	printf("PAT: %0x %0x %0x %0x %0x %0x %0x %0x\n", (unsigned int)data[0], (unsigned int)data[1], (unsigned int)data[2], (unsigned int)data[3], (unsigned int)data[4], (unsigned int)data[5], (unsigned int)data[6], (unsigned int)data[7]);
	assert(PAT_TID_PAS == data[0]); // table_id
	assert(1 == ((data[1] >> 7) & 0x01)); // section_syntax_indicator
//	uint32_t zero = (data[1] >> 6) & 0x01;
//	uint32_t reserved = (data[1] >> 4) & 0x03;
	section_length = ((data[1] & 0x0F) << 8) | data[2];
	transport_stream_id = (data[3] << 8) | data[4];
//	uint32_t reserved2 = (data[5] >> 6) & 0x03;
	version_number = (data[5] >> 1) & 0x1F;
//	uint32_t current_next_indicator = data[5] & 0x01;
//	uint32_t sector_number = data[6];
//	uint32_t last_sector_number = data[7];

	if (PAT_TID_PAS != data[0] || section_length + 3 < 8 + 4 /*crc32*/ || section_length + 3 > bytes)
	{
		assert(0);
		return 0; // invalid data length
	}

	assert(bytes >= section_length + 3); // PMT = section_length + 3
	if(pat->ver != version_number)
		pat->pmt_count = 0; // clear all pmts
	pat->tsid = transport_stream_id;
	pat->ver = version_number;

    // TODO: version_number change, reload pmts

	// 4:CRC, 5:follow section_length item
	for(i = 8; i + 4 <= section_length + 8 - 5 - 4/*CRC32*/ && section_length + 3 <= bytes; i += 4)
	{
        pn = (data[i] << 8) | data[i+1];
        pid = ((data[i+2] & 0x1F) << 8) | data[i+3];
//        printf("PAT: pn: %0x, pid: %0x\n", (unsigned int)pn, (unsigned int)pid);
        
        if(0 == pn)
            continue; // ignore NIT info
        pmt = pat_fetch(pat, pid);
        if(NULL == pmt)
            continue;
        
        pmt->pn = pn;
		pmt->pid = pid;
	}

	//assert(i+4 == bytes);
	//crc = (data[i] << 24) | (data[i+1] << 16) | (data[i+2] << 8) | data[i+3];
	//crc = mpeg_crc32(-1, data, bytes-4);
//	assert(0 == mpeg_crc32(0xffffffff, data, section_length+3));
	return section_length + 3;
}

size_t pat_write(const struct pat_t *pat, uint8_t *data)
{
	// Table 2-30 Program association section(p65)

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

struct pmt_t* pat_find(struct pat_t* pat, uint16_t pn)
{
    unsigned int i;
    for(i = 0; i < pat->pmt_count; i++)
    {
        if(pat->pmts[i].pn == pn)
            return &pat->pmts[i];
    }
    return NULL;
}
