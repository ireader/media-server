// ITU-T H.222.0(10/2014)
// Information technology - Generic coding of moving pictures and associated audio information: Systems
// 2.5.4 Program stream map(p82)

#include "mpeg-ps-internal.h"
#include "mpeg-pes-internal.h"
#include "mpeg-element-descriptor.h"
#include "mpeg-util.h"
#include <assert.h>
#include <string.h>

static struct pes_t* psm_fetch(struct psm_t* psm, uint8_t sid)
{
	size_t i;
	for (i = 0; i < psm->stream_count; i++)
	{
		if (psm->streams[i].sid == sid)
			return &psm->streams[i];
	}

	if (psm->stream_count >= sizeof(psm->streams) / sizeof(psm->streams[0]))
	{
		assert(0);
		return NULL;
	}

	// new stream
	return &psm->streams[psm->stream_count++];
}

#if 1
int psm_read(struct psm_t* psm, struct mpeg_bits_t* reader)
{
	uint8_t v8;
	size_t end, off;
	struct pes_t* stream;
	//uint8_t current_next_indicator;
	uint8_t single_extension_stream_flag;
	uint16_t program_stream_map_length;
	uint16_t program_stream_info_length;
	uint16_t element_stream_map_length;
	uint16_t element_stream_info_length;
	uint8_t cid, sid;

	// Table 2-41 - Program stream map(p79)
	program_stream_map_length = mpeg_bits_read16(reader); // (data[4] << 8)  | data[5];
	end = mpeg_bits_tell(reader) + program_stream_map_length;
	if (mpeg_bits_error(reader) || end > mpeg_bits_length(reader))
		return MPEG_ERROR_NEED_MORE_DATA;

	v8 = mpeg_bits_read8(reader); // data[6]
	//assert((0x20 & data[6]) == 0x00); // 'xx0xxxxx'
	//current_next_indicator = (data[6] >> 7) & 0x01;
	single_extension_stream_flag = (uint8_t)(v8 >> 6) & 0x01; //(data[6] >> 6) & 0x01;
	psm->ver = v8 & 0x1F;
	mpeg_bits_read8(reader); //assert(data[7] == 0x01); // '00000001'

	// program stream descriptor
	program_stream_info_length = mpeg_bits_read16(reader); //(data[8] << 8) | data[9];
	if ((uint32_t)program_stream_info_length + 4 + 2 /*element_stream_map_length*/ > (uint32_t)program_stream_map_length)
		return MPEG_ERROR_INVALID_DATA;

	// TODO: parse descriptor
	//for (i = 10; i + 2 <= 10 + program_stream_info_length;)
	//{
	//	// descriptor()
	//	i += mpeg_elment_descriptor(data + i, 10 + program_stream_info_length - i);
	//}
	mpeg_bits_skip(reader, program_stream_info_length); // 10 + program_stream_info_length;

	// program element stream
	element_stream_map_length = mpeg_bits_read16(reader);
	/* Ignore es_map_length, trust psm_length */
	element_stream_map_length = program_stream_map_length - program_stream_info_length - 10;
	end = mpeg_bits_tell(reader) + element_stream_map_length;

	while (0 == mpeg_bits_error(reader) 
		&& mpeg_bits_tell(reader) + 4 /*element_stream_info_length*/ <= end
		&& psm->stream_count < sizeof(psm->streams) / sizeof(psm->streams[0]))
	{
		cid = mpeg_bits_read8(reader);
		sid = mpeg_bits_read8(reader);
		element_stream_info_length = mpeg_bits_read16(reader);
		if (mpeg_bits_tell(reader) + element_stream_info_length > end)
			return MPEG_ERROR_INVALID_DATA;

		stream = psm_fetch(psm, sid); // sid
		if (NULL == stream)
			continue;
		stream->codecid = cid;
		stream->sid = sid;
		stream->pid = stream->sid; // for ts PID

		off = mpeg_bits_tell(reader);
		if (0xFD == stream->sid && 0 == single_extension_stream_flag)
		{
			if (element_stream_info_length < 3)
				return MPEG_ERROR_INVALID_DATA;
			//uint8_t pseudo_descriptor_tag = mpeg_bits_read8(reader);
			//uint8_t pseudo_descriptor_length = mpeg_bits_read8(reader)
			//uint8_t element_stream_id_extension = mpeg_bits_read8(reader) & 0x7F;
			//assert((0x80 & data[k + 2]) == 0x80); // '1xxxxxxx'
			mpeg_bits_skip(reader, 3);
		}

		while (0 == mpeg_bits_error(reader) && mpeg_bits_tell(reader) < off + element_stream_info_length)
		{
			// descriptor()
			mpeg_elment_descriptor(reader);
		}

		assert(mpeg_bits_tell(reader) == off + element_stream_info_length);
		mpeg_bits_seek(reader, off + element_stream_info_length); // make sure
	}

	mpeg_bits_read32(reader); // crc32
	// assert(j+4 == program_stream_map_length+6);
	// assert(0 == mpeg_crc32(0xffffffff, data, program_stream_map_length+6));
	assert(0 == mpeg_bits_error(reader));
	assert(end + 4 /*crc32*/ == mpeg_bits_tell(reader));
	return MPEG_ERROR_OK;
}

#else
size_t psm_read(struct psm_t *psm, const uint8_t* data, size_t bytes)
{
	size_t i, j, k;
	struct pes_t* stream;
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
	for(j = i; j + 4/*element_stream_info_length*/ <= i+element_stream_map_length && psm->stream_count < sizeof(psm->streams)/sizeof(psm->streams[0]); j += 4 + element_stream_info_length)
	{
		element_stream_info_length = (data[j + 2] << 8) | data[j + 3];
		if (j + 4 + element_stream_info_length > i + element_stream_map_length)
			return 0; // TODO: error

		stream = psm_fetch(psm, data[j + 1]); // sid
		if (NULL == stream)
			continue;
		stream->codecid = data[j];
		stream->sid = data[j+1];
		stream->pid = stream->sid; // for ts PID

		k = j + 4;
		if(0xFD == stream->sid && 0 == single_extension_stream_flag)
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

		assert(k - j - 4 == element_stream_info_length);
	}

//	assert(j+4 == program_stream_map_length+6);
//	assert(0 == mpeg_crc32(0xffffffff, data, program_stream_map_length+6));
	return program_stream_map_length+6;
}
#endif

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
