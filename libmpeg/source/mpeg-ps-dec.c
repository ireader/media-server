// ITU-T H.222.0(06/2012)
// Information technology 每 Generic coding of moving pictures and associated audio information: Systems
// 2.5.3.1 Program stream(p74)

#include <stdio.h>
#include <stdlib.h>
#include "mpeg-ps.h"
#include "mpeg-ps-proto.h"
#include "mpeg-pes-proto.h"
#include <assert.h>
#include <memory.h>

struct mpeg_ps_unpacker_t
{
	ps_system_header_t syshd;
	psm_t psm;

	struct mpeg_ps_func_t *func;
	void* param;	
};

static size_t ps_system_header_dec(const uint8_t* data, size_t bytes, ps_system_header_t *syshd)
{
	int i,j;
	size_t len;

	// 2.5.3.5 System header
	// Table 2-40 每 Program stream system header
	assert(0x00==data[0] && 0x00==data[1] && 0x01==data[2] && 0xBB==data[3]);
	len = (data[4] << 8) | data[5];
	assert(len + 6 <= bytes);

	assert((0x80 & data[6]) == 0x80); // '1xxxxxxx'
	assert((0x01 & data[8]) == 0x01); // 'xxxxxxx1'
	syshd->rate_bound = ((data[6] & 0x7F) << 15) | (data[7] << 7) | ((data[8] >> 1) & 0x7F);

	syshd->audio_bound = (data[9] >> 2) & 0x3F;
	syshd->fixed_flag = (data[9] >> 1) & 0x01;
	syshd->CSPS_flag = (data[9] >> 0) & 0x01;

	assert((0x20 & data[10]) == 0x20); // 'xx1xxxxx'
	syshd->system_audio_lock_flag = (data[10] >> 7) & 0x01;
	syshd->system_video_lock_flag = (data[10] >> 6) & 0x01;
	syshd->video_bound = data[10] & 0x1F;

//	assert((0x7F & data[11]) == 0x00); // 'x0000000'
	syshd->packet_rate_restriction_flag = (data[11] >> 7) & 0x01;

	i = 12;
	for(j = 0; (data[i] & 0x80) == 0x80 && j < NSTREAM; j++)
	{
		syshd->streams[j].stream_id = data[i++];
		if(syshd->streams[j].stream_id == 0xB7) // '10110111'
		{
			assert(data[i] == 0xC0); // '11000000'
			assert((data[i+1] & 80) == 0); // '1xxxxxxx'
			syshd->streams[j].stream_id = (syshd->streams[j].stream_id << 7) | (data[i+1] & 0x7F);
			assert(data[i+2] == 0xB6); // '10110110'
			i += 3;
		}

		assert((data[i] & 0xC0) == 0xC0); // '11xxxxxx'
		syshd->streams[j].buffer_bound_scale = (data[i] >> 5) & 0x01;
		syshd->streams[j].buffer_size_bound = (data[i] & 0x1F) | data[i+1];
		i += 2;
	}

	return len + 4 + 2;
}

size_t mpeg_ps_unpacker_input(void* ptr, const uint8_t* data, size_t bytes)
{
	size_t i=0, n=0, len, c;
	pes_t pes;
	ps_packet_header_t pkhd;
	struct mpeg_ps_unpacker_t *unpacker;
	uint8_t *packet;

	unpacker = (struct mpeg_ps_unpacker_t*)ptr;
	packet = unpacker->func->alloc(unpacker->param, bytes);
	if(!packet || bytes < 4) 
		return bytes; // TODO: check return

	memset(&pkhd, 0, sizeof(pkhd));
	// 2.5.3.3 Pack layer of program stream
	// Table 2-38 每 Program stream pack
	// Table 2-39 每 Program stream pack header
	if (0x00==data[0] && 0x00==data[1] && 0x01==data[2] && PES_SID_START==data[3])
	{
		//assert(0x00==data[0] && 0x00==data[1] && 0x01==data[2] && PES_SID_START==data[3]);
		assert((0x44 & data[4]) == 0x44); // '01xxx1xx'
		assert((0x04 & data[6]) == 0x04); // 'xxxxx1xx'
		assert((0x04 & data[8]) == 0x04); // 'xxxxx1xx'
		assert((0x01 & data[9]) == 0x01); // 'xxxxxxx1'
		pkhd.system_clock_reference_base = (((uint64_t)(data[4] >> 3) & 0x07) << 30) | (((uint64_t)data[4] & 0x3) << 28) | ((uint64_t)data[5] << 20) | ((((uint64_t)data[6] >> 3) & 0x1F) << 15) | (((uint64_t)data[6] & 0x3) << 13) | ((uint64_t)data[7] << 5) | ((data[8] >> 3) & 0x1F);
		pkhd.system_clock_reference_extension = ((data[8] & 0x3) << 7) | ((data[9] >> 1) & 0x7F);

		assert((0x03 & data[12]) == 0x03); // 'xxxxxx11'
		pkhd.program_mux_rate = (data[10] << 14) | (data[11] << 6) | ((data[12] >> 2) & 0x3F);

		//assert((0xF8 & data[13]) == 0x00); // '00000xxx'
		len = data[13] & 0x07; // stuffing

		i = 14 + len;
		assert(0x00==data[i] && 0x00==data[i+1] && 0x01==data[i+2]);
		if(PES_SID_SYS == data[i+3])
		{
			i += ps_system_header_dec(data + i, bytes - i, &unpacker->syshd);
			assert(0x00==data[i] && 0x00==data[i+1] && 0x01==data[i+2]);
		}
	}

	// MPEG_program_end_code = 0x000000B9
	memset(&pes, 0, sizeof(pes));
	while(i + 3 < bytes && 0x00==data[i] && 0x00==data[i+1] && 0x01==data[i+2] 
			&& PES_SID_END != data[i+3] 
			&& PES_SID_START != data[i+3]
			&& (0 == pes.sid || pes.sid == data[i+3])) // the same pes stream(video/audio combine)
	{
		uint16_t len2;
		pes.payload = packet + n;
		pes.payload_len = 0;
		len2 = (data[i+4] << 8) | data[i+5];
		assert((size_t)len2 + 6 <= bytes - i);
		pes_read(data + i, len2 + 6, &unpacker->psm, &pes);

		n += pes.payload_len;
		i += len2 + 6;
	}

	// find stream type
	for(c = 0; c < unpacker->psm.stream_count; ++c)
	{
		if(unpacker->psm.streams[c].pesid == pes.sid)
		{
			pes.avtype = unpacker->psm.streams[c].avtype;
			break;
		}
	}

	unpacker->func->write(unpacker->param, (int)pes.avtype, packet, n);
	unpacker->func->free(unpacker->param, packet);

	return i + ((i+3 < bytes && PES_SID_END==data[i+3]) ? 4 : 0);
}

void* mpeg_ps_unpacker_create(struct mpeg_ps_func_t *func, void* param)
{
	struct mpeg_ps_unpacker_t* unpacker;
	unpacker = malloc(sizeof(struct mpeg_ps_unpacker_t));
	if(!unpacker)
		return NULL;

	memset(unpacker, 0, sizeof(struct mpeg_ps_unpacker_t));
	unpacker->func = func;
	unpacker->param = param;
	return unpacker;
}

int mpeg_ps_unpacker_destroy(void* unpacker)
{
	struct mpeg_ps_unpacker_t* ptr;
	ptr = (struct mpeg_ps_unpacker_t*)unpacker;
	free(ptr);
	return 0;
}
