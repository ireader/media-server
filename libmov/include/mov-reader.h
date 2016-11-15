#ifndef _mov_reader_h_
#define _mov_reader_h_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void* mov_reader_create(const char* file);
void mov_reader_destroy(void* mov);

int mov_reader_read(void* mov, void* buffer, size_t bytes);

#ifdef __cplusplus
}
#endif
#endif /* !_mov_reader_h_*/
