#include "hls-block.h"
#include "hls-param.h"
#include "http-server.h"
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <assert.h>

struct hls_block_t* hls_block_alloc()
{
    struct hls_block_t *block;
    
    block = (struct hls_block_t*)malloc(sizeof(block[0]));
    if(!block)
        return NULL;
    
    memset(block, 0, sizeof(block[0]));
    block->bundle = http_bundle_alloc(HLS_BLOCK_SIZE); // alloc item
    if(!block->bundle)
    {
        free(block);
        return NULL;
    }

    block->ptr = http_bundle_lock(block->bundle);
    return block;
}

void hls_block_free(struct hls_block_t* block)
{
    if(block->bundle)
        http_bundle_free(block->bundle);
    free(block);
}

void hls_block_write(struct hls_block_t* block, const void* packet, size_t bytes)
{
    memcpy((char*)block->ptr + block->len, packet, bytes);
    block->len += bytes;

    http_bundle_unlock(block->bundle, block->len);
}
