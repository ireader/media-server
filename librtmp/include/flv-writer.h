#ifndef _flv_writer_h_
#define _flv_writer_h_

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

void* flv_writer_create(const char* file);

void flv_writer_destroy(void* flv);

/// @param[in] data AAC ADTS stream, 0xFFF15C40011FFC...
int flv_writer_audio(void* flv, const void* data, size_t bytes, uint32_t pts, uint32_t dts);

// @param[in] data H.264 start code + H.264 NALU, 0x0000000168...
int flv_writer_video(void* flv, const void* data, size_t bytes, uint32_t pts, uint32_t dts);

#if defined(__cplusplus)
}
#endif
#endif /* !_flv_writer_h_ */
