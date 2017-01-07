#ifndef _mp4_writer_h_
#define _mp4_writer_h_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void* mov_writer_create(const char* file);
void mov_writer_destroy(void* mov);

int mov_writer_write(void* mov, void* buffer, size_t bytes);

#ifdef __cplusplus
}
#endif
#endif /* !_mp4_writer_h_ */
