#ifndef _hls_block_h_
#define _hls_block_h_

#include "list.h"
#include "ctypedef.h"

struct hls_block_t
{
    struct list_head link;

    void* bundle;
    void* ptr;
    size_t len;
};

struct hls_block_t* hls_block_alloc();
void hls_block_free(struct hls_block_t* block);

void hls_block_write(struct hls_block_t* block, const void* packet, size_t bytes);

#endif /* !_hls_block_h_ */
