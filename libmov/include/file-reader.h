#ifndef _file_reader_h_
#define _file_reader_h_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

	void* file_reader_create(const char* file);
	void file_reader_destroy(void** file);

	/// @return 0-ok/eof, other-error
	int file_reader_error(void* file);
	
	/// @return 0-if error or end-of-file, other-if don't return bytes, check error with file_reader_error
	size_t file_reader_read(void* file, void* buffer, size_t bytes);

	int file_reader_seek(void* file, uint64_t bytes);
	uint64_t file_reader_tell(void* file);

	unsigned int file_reader_r8(void* file);
	
	// little-endian
	unsigned int file_reader_rl16(void* file);
	unsigned int file_reader_rl24(void* file);
	unsigned int file_reader_rl32(void* file);
	uint64_t file_reader_rl64 (void* file);

	// big-endian
	unsigned int file_reader_rb16(void* file);
	unsigned int file_reader_rb24(void* file);
	unsigned int file_reader_rb32(void* file);
	uint64_t file_reader_rb64(void* file);

#ifdef __cplusplus
}
#endif
#endif /* !_file_reader_h_ */
