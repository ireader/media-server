#ifndef _file_writer_h_
#define _file_writer_h_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

	void* file_writer_create(const char* file);
	void file_writer_destroy(void** file);

	/// @return 0-ok/eof, other-error
	int file_writer_error(void* file);

	/// @return 0-if error, other-if don't return bytes, check error with file_reader_error
	size_t file_writer_write(void* file, const void* buffer, size_t bytes);

	int file_writer_seek(void* file, uint64_t offset);
	uint64_t file_writer_tell(void* file);

	void file_writer_w8(void* file, uint8_t value);

	// little-endian
	void file_writer_wl16(void* file, uint16_t value);
	void file_writer_wl24(void* file, uint32_t value);
	void file_writer_wl32(void* file, uint32_t value);
	void file_writer_wl64(void* file, uint64_t value);

	// big-endian
	void file_writer_wb16(void* file, uint16_t value);
	void file_writer_wb24(void* file, uint32_t value);
	void file_writer_wb32(void* file, uint32_t value);
	void file_writer_wb64(void* file, uint64_t value);

#ifdef __cplusplus
}
#endif
#endif /* !_file_writer_h_ */
