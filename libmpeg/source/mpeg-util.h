#ifndef _mpeg_util_h_
#define _mpeg_util_h_

#include "mpeg-types.h"

void put16(uint8_t *ptr, uint32_t val);
void put32(uint8_t *ptr, uint32_t val);

void pcr_write(uint8_t *ptr, int64_t pcr);

#endif /* !_mpeg_util_h_ */
