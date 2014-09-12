// ITU-T H.222.0(06/2012)
// Information technology ¨C Generic coding of moving pictures and associated audio information: Systems
// 2.5.3.1 Program stream(p74)

#include "mpeg-ps-proto.h"
#include "mpeg-util.h"
#include "mpeg-ps.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

size_t ps_system_header_write(const ps_system_header_t *syshd, uint8_t *data)
{
	int i, j;

	// system_header_start_code
	put32(data, 0x000001BB);

	// header length
	put16(data + 4, 0);

	// rate_bound
	// 1xxxxxxx xxxxxxxx xxxxxxx1
	data[6] = 0x80 | ((syshd->rate_bound >> 15) & 0x7F);
	data[7] = (syshd->rate_bound >> 7) & 0xFF;
	data[8] = 0x01 | (syshd->rate_bound & 0x7F);

	// 6-audio_bound + 1-fixed_flag + 1-CSPS_flag
	data[9] = ((syshd->audio_bound & 0x3F) << 6) | ((syshd->fixed_flag & 0x01) << 1) | (syshd->CSPS_flag & 0x01);

	// 1-system_audio_lock_flag + 1-system_video_lock_flag + 1-maker + 5-video_bound
	data[10] = 0x20 | ((syshd->system_audio_lock_flag & 0x01) << 7) | ((syshd->video_bound & 0x01) << 6) | (syshd->video_bound & 0x1F);

	// 1-packet_rate_restriction_flag + 7-reserved
	data[11] = (syshd->packet_rate_restriction_flag & 0x01) << 7;

	i = 11;
	for(j = 0; j < syshd->stream_count; j++)
	{
		data[++i] = syshd->streams[j].stream_id;
		if(0xB7 == syshd->streams[j].stream_id) // '10110111'
		{
			data[++i] = 0xD0; // '11000000'
			data[++i] = syshd->streams[j].stream_extid & 0x7F; // '0xxxxxxx'
			data[++i] = 0xB6; // '10110110'
		}

		// '11' + 1-P-STD_buffer_bound_scale + 13-P-STD_buffer_size_bound
		// '11xxxxxx xxxxxxxx'
		data[++i] = 0xC0 | ((syshd->streams[j].buffer_bound_scale & 0x01) << 5) | ((syshd->streams[j].buffer_size_bound >> 8) & 0x1F);
		data[++i] = syshd->streams[j].buffer_size_bound & 0xFF;
	}

	return i+1;
}

size_t ps_packet_header_write(const ps_packet_header_t *packethd, uint8_t *data)
{
	// pack_start_code
	put32(data, 0x000001BA);

	// 33-system_clock_reference_base + 9-system_clock_reference_extension
	// '01xxx1xx xxxxxxxx xxxxx1xx xxxxxxxx xxxxx1xx xxxxxxx1'
	data[4] = 0x40 | (((packethd->system_clock_reference_base >> 30) & 0x07) << 3) | ((packethd->system_clock_reference_base >> 28) & 0x03);
	data[5] = ((packethd->system_clock_reference_base >> 20) & 0xFF);
	data[6] = 0x04 | (((packethd->system_clock_reference_base >> 15) & 0x1F) << 3) | ((packethd->system_clock_reference_base >> 13) & 0x03);
	data[7] = ((packethd->system_clock_reference_base >> 5) & 0xFF);
	data[8] = 0x04 | ((packethd->system_clock_reference_base & 0x1F) << 3) | ((packethd->system_clock_reference_extension >> 7) & 0x03);
	data[9] = 0x01 | ((packethd->system_clock_reference_extension & 0x7F) << 1);

	// program_mux_rate
	// 'xxxxxxxx xxxxxxxx xxxxxx11'
	data[10] = packethd->program_mux_rate >> 14;
	data[11] = packethd->program_mux_rate >> 6;
	data[12] = 0x03 | ((packethd->program_mux_rate & 0x3F) << 2);

	// stuffing length
	// '00000xxx'
	data[13] =  0;

	return 14;
}
