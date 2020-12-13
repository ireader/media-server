#ifndef _webm_buffer_h_
#define _webm_buffer_h_

#include <stdint.h>

struct webm_buffer_t
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

	/// webme buffer position
	/// @param[in] param user-defined parameter
	/// @param[in] offset seek buffer read/write position to offset(from buffer begin)
	/// @return 0-ok, <0-error
	int (*seek)(void* param, uint64_t offset);

	/// get buffer read/write position
	/// @return >=0-current read/write position, <0-error
	uint64_t (*tell)(void* param);
};

#endif /* !_webm_buffer_h_ */
