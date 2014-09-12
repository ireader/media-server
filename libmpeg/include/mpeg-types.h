#ifndef _mpeg_types_h_
#define _mpeg_types_h_

#include <stdio.h>
#include <stdlib.h>

#define PTS_NO_VALUE (int64_t)0x8000000000000000L

typedef unsigned char	uint8_t;
typedef short			int16_t;
typedef unsigned short	uint16_t;
typedef int				int32_t;
typedef unsigned int	uint32_t;

#if defined(OS_WINDOWS)
	typedef __int64				int64_t;
	typedef unsigned __int64	uint64_t;
#else
	typedef long long			int64_t;
	typedef unsigned long long	uint64_t;
#endif

#endif /* !_mpeg_types_h_ */
