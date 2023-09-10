#ifndef _mpeg_types_h_
#define _mpeg_types_h_

#include <inttypes.h>
#include <stdint.h>
#include <stddef.h>

#define PTS_NO_VALUE INT64_MIN //(int64_t)0x8000000000000000L

enum { MPEG_VCL_NONE = 0, MPEG_VCL_IDR, MPEG_VCL_P, MPEG_VCL_CORRUPT };

#endif /* !_mpeg_types_h_ */
