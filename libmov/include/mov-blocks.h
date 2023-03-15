#ifndef _mov_blocks_h_
#define _mov_blocks_h_

#include <stdint.h>

struct mov_blocks_t
{
	/// create a blocks
    /// @param[in] param       user-defined parameter
    /// @param[in] id          blocks identifier
    /// @param[in] block_size  single block size
    /// @return 0-ok, <0-error
	int  (*create)(void* param, uint32_t id, uint32_t block_size);

	/// destroy a blocks
    /// @param[in]  param user-defined parameter
    /// @param[in]  id    blocks identifier
    /// @return 0-ok, <0-error
	int  (*destroy)(void* param, uint32_t id);

	/// set blocks capacity
    /// @param[in]  param     user-defined parameter
    /// @param[in]  id        blocks identifier
    /// @param[in]  capacity  blocks capacity
	int  (*set_capacity)(void* param, uint32_t id, uint64_t capacity);

	/// get block by index
    /// @param[in]  param   user-defined parameter
    /// @param[in]  id      blocks identifier
    /// @param[in]  index   block index in blocks
    /// @return     memory pointer
	void* (*at)(void* param, uint32_t id, uint64_t index);
};

#endif /*! _mov_blocks_h_ */