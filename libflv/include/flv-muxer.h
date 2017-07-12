#ifndef _flv_muxer_h_
#define _flv_muxer_h_

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

///Video: FLV VideoTagHeader + AVCVIDEOPACKET: AVCDecoderConfigurationRecord(ISO 14496-15) / One or more NALUs(four-bytes length + NALU)
///Audio: FLV AudioTagHeader + AACAUDIODATA: AudioSpecificConfig(14496-3) / Raw AAC frame data in UI8
///@param[in] data FLV Audio/Video Data(don't include FLV Tag Header)
///@param[in] type 8-audio, 9-video
///@return 0-ok, other-error
typedef int (*flv_muxer_handler)(void* param, int type, const void* data, size_t bytes, uint32_t timestamp);

void* flv_muxer_create(flv_muxer_handler handler, void* param);
void flv_muxer_destroy(void* flv);

/// re-create AAC/AVC sequence header
int flv_muxer_reset(void* flv);

/// @param[in] data AAC ADTS stream, 0xFFF15C40011FFC...
int flv_muxer_aac(void* flv, const void* data, size_t bytes, uint32_t pts, uint32_t dts);

/// @param[in] data mp4 stream
int flv_muxer_mp3(void* flv, const void* data, size_t bytes, uint32_t pts, uint32_t dts);

// @param[in] data H.264 start code + H.264 NALU, 0x0000000168...
int flv_muxer_avc(void* flv, const void* data, size_t bytes, uint32_t pts, uint32_t dts);

#if defined(__cplusplus)
}
#endif
#endif /* !_flv_muxer_h_ */
