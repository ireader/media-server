#ifndef _mpeg_util_h_
#define _mpeg_util_h_

#include "mpeg-types.h"
#include "byte-order.h"

void pcr_write(uint8_t *ptr, int64_t pcr);

#endif /* !_mpeg_util_h_ */
