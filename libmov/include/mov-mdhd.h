#ifndef _mov_mdhd_h_
#define _mov_mdhd_h_

#include <stdint.h>
#include <stddef.h>

struct mov_mdhd_t
{
	// FullBox
	uint32_t version : 8;
	uint32_t flags : 24;

	uint64_t creation_time; // seconds sine midnight, Jan. 1, 1904, UTC
	uint64_t modification_time; // seconds sine midnight, Jan. 1, 1904, UTC
	uint64_t duration; // default 1s
	uint32_t timescale; // second

	uint32_t pad : 1;
	uint32_t language : 15;
	uint32_t pre_defined : 16;
};

#endif /* !_mov_mdhd_h_ */
