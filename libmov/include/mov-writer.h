#ifndef _mp4_writer_h_
#define _mp4_writer_h_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void* mov_writer_create(const char* file);
void mov_writer_destroy(void* mov);

// MOV_AVC1/MOV_MP4A
/// @param[in] extra_data AudioSpecificConfig/AVCDecoderConfigurationRecord
int mov_writer_audio_meta(void* mov, uint32_t avtype, int channel_count, int bits_per_sample, int sample_rate, const void* extra_data, size_t extra_data_size);
int mov_writer_video_meta(void* mov, uint32_t avtype, int width, int height, const void* extra_data, size_t extra_data_size);

// raw AAC data, don't include ADTS/AudioSpecificConfig
int mov_writer_write_audio(void* mov, const void* buffer, size_t bytes, int64_t pts, int64_t dts);

// H.264 NALU, don't include 0x00000001
int mov_writer_write_video(void* mov, const void* buffer, size_t bytes, int64_t pts, int64_t dts);

#ifdef __cplusplus
}
#endif
#endif /* !_mp4_writer_h_ */
