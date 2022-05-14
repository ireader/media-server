// ITU-T H.222.0(10/2014)
// Information technology - Generic coding of moving pictures and associated audio information: Systems
// 2.5.4 Program stream map(p82)

#include "mpeg-ps-proto.h"
#include "mpeg-pes-proto.h"
#include "mpeg-element-descriptor.h"
#include "mpeg-util.h"
#include <assert.h>
#include <string.h>

size_t psm_read(struct psm_t *psm, const uint8_t* data, size_t bytes)
{
	size_t i, j, k;
	//uint8_t current_next_indicator;
	uint8_t single_extension_stream_flag;
	uint16_t program_stream_map_length;
	uint16_t program_stream_info_length;
	uint16_t element_stream_map_length;
	uint16_t element_stream_info_length;

	// Table 2-41 - Program stream map(p79)
	assert(0x00==data[0] && 0x00==data[1] && 0x01==data[2] && 0xBC==data[3]);
	program_stream_map_length = (data[4] << 8) | data[5];
	if (program_stream_map_length < 3 || bytes < (size_t)program_stream_map_length + 6)
		return 0; // invalid data length

	//assert((0x20 & data[6]) == 0x00); // 'xx0xxxxx'
	//current_next_indicator = (data[6] >> 7) & 0x01;
	single_extension_stream_flag = (data[6] >> 6) & 0x01;
	psm->ver = data[6] & 0x1F;
	//assert(data[7] == 0x01); // '00000001'

	// program stream descriptor
	program_stream_info_length = (data[8] << 8) | data[9];
	if ((size_t)program_stream_info_length + 4 + 2 /*element_stream_map_length*/ > (size_t)program_stream_map_length)
		return 0; // TODO: error

	// TODO: parse descriptor
	//for (i = 10; i + 2 <= 10 + program_stream_info_length;)
	//{
	//	// descriptor()
	//	i += mpeg_elment_descriptor(data + i, 10 + program_stream_info_length - i);
	//}

	// program element stream
	i = 10 + program_stream_info_length;
	element_stream_map_length = (data[i] << 8) | data[i+1];
	 /* Ignore es_map_length, trust psm_length */
	element_stream_map_length = program_stream_map_length - program_stream_info_length - 10;
	
	i += 2;
	psm->stream_count = 0;
	for(j = i; j + 4/*element_stream_info_length*/ <= i+element_stream_map_length && psm->stream_count < sizeof(psm->streams)/sizeof(psm->streams[0]); j += 4 + element_stream_info_length)
	{
		psm->streams[psm->stream_count].codecid = data[j];
		psm->streams[psm->stream_count].sid = data[j+1];
		psm->streams[psm->stream_count].pid = psm->streams[psm->stream_count].sid; // for ts PID
		element_stream_info_length = (data[j+2] << 8) | data[j+3];
		if (j + 4 + element_stream_info_length > i+element_stream_map_length)
			return 0; // TODO: error

		k = j + 4;
		if(0xFD == psm->streams[psm->stream_count].sid && 0 == single_extension_stream_flag)
		{
			if(element_stream_info_length < 3)
				return 0; // TODO: error 
//			uint8_t pseudo_descriptor_tag = data[k];
//			uint8_t pseudo_descriptor_length = data[k+1];
//			uint8_t element_stream_id_extension = data[k+2] & 0x7F;
			assert((0x80 & data[k+2]) == 0x80); // '1xxxxxxx'
			k += 3;
		}

		while(k + 2 <= j + 4 + element_stream_info_length)
		{
			// descriptor()
			k += mpeg_elment_descriptor(data+k, j + 4 + element_stream_info_length - k);
		}

		++psm->stream_count;
		assert(k - j - 4 == element_stream_info_length);
	}

//	assert(j+4 == program_stream_map_length+6);
//	assert(0 == mpeg_crc32(0xffffffff, data, program_stream_map_length+6));
	return program_stream_map_length+6;
}

size_t psm_write(const struct psm_t *psm, uint8_t *data)
{
	// Table 2-41 - Program stream map(p79)

	size_t i,j;
	uint16_t extlen;
	unsigned int crc;

	nbo_w32(data, 0x00000100);
	data[3] = PES_SID_PSM;

	// program_stream_map_length 16-bits
	//nbo_w16(data+4, 6+4*psm->stream_count+4);

	// current_next_indicator '1'
	// single_extension_stream_flag '1'
	// reserved '0'
	// program_stream_map_version 'xxxxx'
	data[6] = 0xc0 | (psm->ver & 0x1F);

	// reserved '0000000'
	// marker_bit '1'
	data[7] = 0x01;

	extlen = 0;
	extlen += (uint16_t)service_extension_descriptor_write(data + 10 + extlen, 32);
#if defined(MPEG_CLOCK_EXTENSION_DESCRIPTOR)
	extlen += (uint16_t)clock_extension_descriptor_write(data + 10 + extlen, 32, psm->clock);
#endif

	// program_stream_info_length 16-bits
	nbo_w16(data + 8, extlen); // program_stream_info_length = 0

	// elementary_stream_map_length 16-bits
	//nbo_w16(data+10+extlen, psm->stream_count*4);

	j = 12 + extlen;
	for(i = 0; i < psm->stream_count; i++)
	{
		assert(PES_SID_EXTEND != psm->streams[i].sid);

		// stream_type:8
		data[j++] = psm->streams[i].codecid;
		// elementary_stream_id:8
		data[j++] = psm->streams[i].sid;
		// elementary_stream_info_length:16
		nbo_w16(data+j, psm->streams[i].esinfo_len);
		// descriptor()
		memcpy(data+j+2, psm->streams[i].esinfo, psm->streams[i].esinfo_len);

		j += 2 + psm->streams[i].esinfo_len;
	}

	// elementary_stream_map_length 16-bits
	nbo_w16(data + 10 + extlen, (uint16_t)(j - 12 - extlen));
	// program_stream_map_length:16
	nbo_w16(data + 4, (uint16_t)(j-6+4)); // 4-bytes crc32

	// crc32
	crc = mpeg_crc32(0xffffffff, data, (uint32_t)j);
	data[j+3] = (uint8_t)((crc >> 24) & 0xFF);
	data[j+2] = (uint8_t)((crc >> 16) & 0xFF);
	data[j+1] = (uint8_t)((crc >> 8) & 0xFF);
	data[j+0] = (uint8_t)(crc & 0xFF);

	return j+4;
}
