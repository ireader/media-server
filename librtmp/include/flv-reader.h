#ifndef _flv_reader_h_
#define _flv_reader_h_

#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

void* flv_reader_create(const char* file);
void flv_reader_destroy(void* flv);

int flv_reader_read(void* flv, void* buffer, size_t bytes);

#if defined(__cplusplus)
}
#endif
#endif /* !_flv_reader_h_ */
