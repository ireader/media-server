#ifndef _mov_box_h_
#define _mov_box_h_

#include <stdint.h>
#include <stddef.h>

// ISO/IEC 14496-12:2012(E) 4.2 Object Structure (16)
struct mov_box_t
{
	uint64_t size; // 0-size: box extends to end of file, 1-size: large size
	uint32_t type;

	// if 'uuid' == type
	//uint8_t usertype[16];

	// FullBox
	//uint32_t version : 8;
	//uint32_t flags : 24;

#if defined(DEBUG) || defined(_DEBUG)
	int level;
#endif
};

#endif /* !_mov_box_h_ */
