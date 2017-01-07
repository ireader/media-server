#ifndef _mov_tkhd_h_
#define _mov_tkhd_h_

#include <stdint.h>
#include <stddef.h>

enum thkh_flags {
	TKHD_FLAG_TRACK_ENABLE = 0x01,
	TKHD_FLAG_TRACK_IN_MOVIE = 0x02,
	TKHD_FLAG_TRACK_IN_PREVIEW = 0x04,
};

struct mov_tkhd_t
{
	// FullBox
	uint32_t version : 8;
	uint32_t flags : 24;

	uint32_t track_ID; // cannot be zero
	uint64_t creation_time; // seconds sine midnight, Jan. 1, 1904, UTC
	uint64_t modification_time; // seconds sine midnight, Jan. 1, 1904, UTC
	uint64_t duration; // default 1s	
	//uint32_t reserved;
	
	//uint32_t reserved2[2];
	int16_t layer;
	int16_t alternate_group;
	int16_t volume; // fixed point 8.8 number, 1.0 (0x0100) is full volume
	//uint16_t reserved;	
	int32_t matrix[9]; // u,v,w
	uint32_t width; // fixed-point 16.16 values
	uint32_t height; // fixed-point 16.16 values
};

#endif /* !_mov_tkhd_h_ */
