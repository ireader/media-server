#include "hls-file.h"
#include "cstringext.h"
#include "sys/sync.h"
#include "http-server.h"
#include <stdlib.h>
#include <memory.h>
#include <assert.h>
#include <errno.h>

struct hls_file_t* hls_file_open()
{
	struct hls_file_t* file;

	file = (struct hls_file_t*)malloc(sizeof(file[0]));
	if(file)
	{
		memset(file, 0, sizeof(file[0]));
		file->head.bundle = http_bundle_alloc(BLOCK_SIZE);
		file->head.ptr = http_bundle_lock(file->head.bundle);
		file->tail = &file->head;
		file->refcnt = 1;
		file->tcreate = time64_now();
	}

	return file;
}

int hls_file_close(struct hls_file_t* file)
{
	struct hls_block_t *block, *block2;

	if(0 != InterlockedDecrement(&file->refcnt))
		return 0;

	block = &file->head;
	while(block)
	{
		block2 = block->next;
		http_bundle_free(block->bundle);
		if(block != &file->head)
			free(block);
		block = block2;
	}

	free(file);
	return 0;
}

int hls_file_write(struct hls_file_t* file, const void* packet, int bytes)
{
	struct hls_block_t *block;

	assert(file->tail);
	if(file->tail->len + bytes > BLOCK_SIZE)
	{
		block = (struct hls_block_t*)malloc(sizeof(block[0]));
		if(!block)
			return ENOMEM;

		memset(block, 0, sizeof(block[0]));
		block->bundle = http_bundle_alloc(BLOCK_SIZE);
		block->ptr = http_bundle_lock(block->bundle);
		file->tail->next = block; // link
		file->tail = block;
	}

	memcpy((char*)file->tail->ptr + file->tail->len, packet, bytes);
	file->tail->len += bytes;
	http_bundle_unlock(file->tail->bundle, file->tail->len);

	return 0;
}
