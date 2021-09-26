#ifndef _mov_buffer_h_
#define _mov_buffer_h_

#include <stdint.h>

struct mov_buffer_t
{
	/// read data from buffer
	/// @param[in] param user-defined parameter
	/// @param[out] data user buffer
	/// @param[in] bytes data buffer size
	/// @return 0-ok, <0-error
	int (*read)(void* param, void* data, uint64_t bytes);

	/// write data to buffer
	/// @param[in] param user-defined parameter
	/// @param[in] data user buffer
	/// @param[in] bytes data buffer size
	/// @return 0-ok, <0-error
	int (*write)(void* param, const void* data, uint64_t bytes);

	/// move buffer position
	/// @param[in] param user-defined parameter
	/// @param[in] offset >=0-seek buffer read/write position to offset(from buffer begin), <0-seek from file end(SEEK_END)
	/// @return 0-ok, <0-error
	int (*seek)(void* param, int64_t offset);

	/// get buffer read/write position
	/// @return <0-error, other-current read/write position
	int64_t (*tell)(void* param);
};

#endif /* !_mov_buffer_h_ */
