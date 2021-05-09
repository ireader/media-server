#ifndef _mov_file_buffer_h_
#define _mov_file_buffer_h_

#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct mov_file_cache_t
{
	FILE* fp;
	uint8_t ptr[800];
	unsigned int len;
	unsigned int off;
	uint64_t tell;
};

const struct mov_buffer_t* mov_file_cache_buffer(void);

const struct mov_buffer_t* mov_file_buffer(void);

#ifdef __cplusplus
}
#endif
#endif /* !_mov_file_buffer_h_ */
