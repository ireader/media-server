#ifndef _mpeg_types_h_
#define _mpeg_types_h_

#include <stdio.h>
#include <stdlib.h>

#define PTS_NO_VALUE (int64_t)0x8000000000000000L

#if defined(OS_WINDOWS)
	typedef unsigned char		bool_t;
	typedef char				int8_t;
	typedef short				int16_t;
	typedef int					int32_t;

	typedef unsigned char		uint8_t;
	typedef unsigned short		uint16_t;
	typedef unsigned int		uint32_t;

	#ifndef OS_INT64_TYPE
	typedef __int64				int64_t;
	typedef unsigned __int64	uint64_t;
	#define PRId64 "I64d"
	#define PRIu64 "I64u"
	#define OS_INT64_TYPE
	#endif /* OS_INT64_TYPE */
#else
	#include <stdint.h>
	#include <inttypes.h>
	#ifndef OS_INT64_TYPE
	typedef long long int		int64_t;
	typedef unsigned long long int uint64_t;
	#define OS_INT64_TYPE
	#endif /* OS_INT64_TYPE */
#endif

#endif /* !_mpeg_types_h_ */
