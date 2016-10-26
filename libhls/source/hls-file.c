#include "hls-file.h"
#include "hls-param.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <memory.h>
#include <assert.h>
#include <errno.h>

struct hls_file_t* hls_file_open()
{
	struct hls_file_t* file;
    struct hls_block_t *block;

	file = (struct hls_file_t*)malloc(sizeof(file[0]));
	if(file)
	{
		memset(file, 0, sizeof(file[0]));
        LIST_INIT_HEAD(&file->head);
        file->seq = 0;
		file->refcnt = 1;
		file->tcreate = time64_now();

        // default alloc 1-block with file
        block = hls_block_alloc();
        if(!block)
        {
            free(file);
            return NULL;
        }

        list_insert_after(&block->link, &file->head);
	}

	return file;
}

int hls_file_close(struct hls_file_t* file)
{
    struct hls_block_t *block;
	struct list_head *pos, *next;

	if(0L == atomic_decrement32(&file->refcnt))
    {
        list_for_each_safe(pos, next, &file->head)
        {
            block = list_entry(pos, struct hls_block_t, link);
            hls_block_free(block);
        }

        free(file);
    }

	return 0;
}

int hls_file_write(struct hls_file_t* file, const void* packet, size_t bytes)
{
	struct hls_block_t *block;

    assert( !list_empty(&file->head) );
    block = list_entry(file->head.prev, struct hls_block_t, link);

    assert(block->len % 188 == 0);
	if(block->len + bytes > HLS_BLOCK_SIZE)
	{
        block = hls_block_alloc();
        if(!block)
            return ENOMEM;

        list_insert_after(&block->link, file->head.prev); // link
	}

    hls_block_write(block, packet, bytes);
	return 0;
}

void hls_file_save(const char* name, struct hls_file_t *file)
{
    struct list_head *pos;
    struct hls_block_t *block;
    FILE* fp;

    fp = fopen(name, "wb");

    list_for_each(pos, &file->head)
    {
        block = list_entry(pos, struct hls_block_t, link);
        fwrite(block->ptr, 1, block->len, fp);
    }

    fclose(fp);
}
