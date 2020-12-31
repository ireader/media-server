#include "avbuffer.h"
#include "sys/atomic.h"
#include <stdlib.h>
#include <assert.h>

struct avbuffer_t* avbuffer_alloc(int size)
{
	struct avbuffer_t* buf;
	buf = malloc(sizeof(*buf) + size);
	if (buf)
	{
		buf->data = (uint8_t*)(buf + 1);
		buf->size = size;
		buf->refcount = 1;
		buf->free = NULL;
		buf->opaque = NULL;
	}
	return buf;
}

int32_t avbuffer_addref(struct avbuffer_t* buf)
{
	if (NULL == buf || NULL == buf->data)
		return -1;
	return atomic_increment32(&buf->refcount);
}

int32_t avbuffer_release(struct avbuffer_t* buf)
{
	int32_t ref;
	if (NULL == buf || NULL == buf->data)
		return -1;
	
	ref = atomic_decrement32(&buf->refcount);
	assert(ref >= 0);
	if (0 == ref)
	{
		if (buf->free)
			buf->free(buf->opaque, buf->data);
		free(buf);
	}
	return ref;
}
