#ifndef _flv_reader_h_
#define _flv_reader_h_

#include <stdint.h>
#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

void* flv_reader_create(const char* file);
void flv_reader_destroy(void* flv);

///@param[out] tagtype 8-audio, 9-video, 18-script data
///@param[out] timestamp FLV timestamp
///@param[out] buffer FLV stream
int flv_reader_read(void* flv, int* tagtype, uint32_t* timestamp, void* buffer, size_t bytes);

#if defined(__cplusplus)
}
#endif
#endif /* !_flv_reader_h_ */
