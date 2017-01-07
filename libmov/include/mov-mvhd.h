#ifndef _mov_mvhd_h_
#define _mov_mvhd_h_

#include <stdint.h>
#include <stddef.h>

struct mov_mvhd_t
{
	// FullBox
	uint32_t version : 8;
	uint32_t flags : 24;

	uint32_t timescale; // second
	uint64_t creation_time; // seconds sine midnight, Jan. 1, 1904, UTC
	uint64_t modification_time; // seconds sine midnight, Jan. 1, 1904, UTC
	uint64_t duration; // default 1s
	
	uint32_t rate;
	uint16_t volume; // fixed point 8.8 number, 1.0 (0x0100) is full volume
	//uint16_t reserved;
	//uint32_t reserved2[2];
	int32_t matrix[9]; // u,v,w
	//int32_t pre_defined[6];
	uint32_t next_track_ID;
};

#endif /* !_mov_mvhd_h_ */
