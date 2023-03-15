#ifndef _mov_blocks_ulti_h_
#define _mov_ioutil_ulti_h_

#include "mov-blocks.h"

struct mov_sample_t;

struct mov_blocks_utli_t
{
    struct mov_blocks_t blocks;
    void* param;
};

static inline int mov_blocks_create(const struct mov_blocks_utli_t* blocks, uint32_t id, uint32_t block_size)
{
    return blocks->blocks.create(blocks->param, id, block_size); 
};

static inline int mov_blocks_destroy(const struct mov_blocks_utli_t* blocks, uint32_t id)
{
    return blocks->blocks.destroy(blocks->param, id);
}

static inline int mov_blocks_set_capacity(const struct mov_blocks_utli_t* blocks, uint32_t id, uint64_t capacity)
{
    return blocks->blocks.set_capacity(blocks->param, id, capacity);
}

static inline void* mov_blocks_at(const struct mov_blocks_utli_t* blocks, uint32_t id, uint64_t index)
{
    return blocks->blocks.at(blocks->param, id, index);
}

static inline struct mov_sample_t* mov_sample_t_at(const struct mov_blocks_utli_t* blocks, uint32_t id, uint64_t index)
{
    return (struct mov_sample_t*)mov_blocks_at(blocks, id, index);
}

#endif /* !_mov_ioutil_ulti_h_ */