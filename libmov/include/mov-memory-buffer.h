#ifndef _mov_memory_buffer_h_
#define _mov_memory_buffer_h_

#include "mov-buffer.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

struct mov_memory_buffer_t
{
	uint8_t* ptr;
	uint64_t off;
	uint64_t capacity;
};

static int mov_memory_read(void* param, void* data, uint64_t bytes)
{
	struct mov_memory_buffer_t* ptr;
	ptr = (struct mov_memory_buffer_t*)param;
	if (ptr->off + bytes > ptr->capacity)
		return -1;

	memcpy(data, ptr->ptr + ptr->off, (size_t)bytes);
	ptr->off += bytes;
	return 0;
}

static int mov_memory_write(void* param, const void* data, uint64_t bytes)
{
	struct mov_memory_buffer_t* ptr;
	ptr = (struct mov_memory_buffer_t*)param;
	if (ptr->off + bytes > ptr->capacity)
		return -1;

	memcpy(ptr->ptr + ptr->off, data, (size_t)bytes);
	ptr->off += bytes;
	return 0;
}

static int mov_memory_seek(void* param, int64_t offset)
{
	struct mov_memory_buffer_t* ptr;
	ptr = (struct mov_memory_buffer_t*)param;
	if ((uint64_t)(offset >= 0 ? offset : -offset) > ptr->capacity)
		return -1;
	ptr->off = offset > 0 ? offset : (ptr->capacity+offset);
	return 0;
}

static int64_t mov_memory_tell(void* param)
{
	struct mov_memory_buffer_t* ptr;
	ptr = (struct mov_memory_buffer_t*)param;
	return (int64_t)ptr->off;
}

static inline const struct mov_buffer_t* mov_memory_buffer(void)
{
	static struct mov_buffer_t s_io = {
		mov_memory_read,
		mov_memory_write,
		mov_memory_seek,
		mov_memory_tell,
	};
	return &s_io;
}

#endif /* !_mov_memory_buffer_h_ */
