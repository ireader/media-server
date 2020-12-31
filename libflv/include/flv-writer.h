#ifndef _flv_writer_h_
#define _flv_writer_h_

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

void* flv_writer_create(const char* file);
void* flv_writer_create2(int (*write)(void* param, const void* buf, int len), void* param);

void flv_writer_destroy(void* flv);

/// Video: FLV VideoTagHeader + AVCVIDEOPACKET: AVCDecoderConfigurationRecord(ISO 14496-15) / One or more NALUs(four-bytes length + NALU)
/// Audio: FLV AudioTagHeader + AACAUDIODATA: AudioSpecificConfig(14496-3) / Raw AAC frame data in UI8
/// @param[in] data FLV Audio/Video Data(don't include FLV Tag Header)
/// @param[in] type 8-audio, 9-video
/// @return 0-ok, other-error
int flv_writer_input(void* flv, int type, const void* data, size_t bytes, uint32_t timestamp);

#if defined(__cplusplus)
}
#endif
#endif /* !_flv_writer_h_ */
