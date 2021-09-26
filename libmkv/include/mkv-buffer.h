#ifndef _mkv_buffer_h_
#define _mkv_buffer_h_

#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct mkv_buffer_t
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

	/// mkve buffer position
	/// @param[in] param user-defined parameter
	/// @param[in] offset >=0-seek buffer read/write position to offset(from buffer begin), <0-seek from file end(SEEK_END)
	/// @return 0-ok, <0-error
	int (*seek)(void* param, int64_t offset);

	/// get buffer read/write position
	/// @return <0-error, other-current read/write position
	int64_t (*tell)(void* param);
};

struct mkv_file_cache_t
{
	FILE* fp;
	uint8_t ptr[800];
	unsigned int len;
	unsigned int off;
	uint64_t tell;
};

const struct mkv_buffer_t* mkv_file_cache_buffer(void);

const struct mkv_buffer_t* mkv_file_buffer(void);

#ifdef __cplusplus
}
#endif
#endif /* !_mkv_buffer_h_ */
