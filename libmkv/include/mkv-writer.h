#ifndef mkv_writer_h
#define mkv_writer_h

#include <stddef.h>
#include <stdint.h>
#include "mkv-format.h"
#include "mkv-buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mkv_writer_t mkv_writer_t;

/// @param[in] options mkv options, such as: MKV_OPTION_LIVE, see more @mkv-format.h
mkv_writer_t* mkv_writer_create(const struct mkv_buffer_t* buffer, void* param, int options);
void mkv_writer_destroy(mkv_writer_t* mkv);

/// @param[in] codec MPEG-4 systems ObjectTypeIndication such as: mkv_OBJECT_H264, see more @mkv-format.h
/// @param[in] extra_data AudioSpecificConfig/AVCDecoderConfigurationRecord/HEVCDecoderConfigurationRecord
/// @return >=0-track, <0-error
int mkv_writer_add_audio(mkv_writer_t* mkv, enum mkv_codec_t codec, int channel_count, int bits_per_sample, int sample_rate, const void* extra_data, size_t extra_data_size);
int mkv_writer_add_video(mkv_writer_t* mkv, enum mkv_codec_t codec, int width, int height, const void* extra_data, size_t extra_data_size);
int mkv_writer_add_subtitle(mkv_writer_t* mkv, enum mkv_codec_t codec, const void* extra_data, size_t extra_data_size);

/// Write audio/video stream
/// raw AAC data, don't include ADTS/AudioSpecificConfig
/// H.264/H.265 MP4 format, replace start code(0x00000001) with NALU size
/// @param[in] track return by mkv_writer_add_audio/mkv_writer_add_video
/// @param[in] data audio/video frame
/// @param[in] bytes buffer size
/// @param[in] pts timestamp in millisecond
/// @param[in] dts timestamp in millisecond
/// @param[in] flags MKV_FLAGS_XXX, such as: MKV_FLAGS_KEYFREAME, see more @mkv-format.h
/// @return 0-ok, other-error
int mkv_writer_write(mkv_writer_t* mkv, int track, const void* data, size_t bytes, int64_t pts, int64_t dts, int flags);

#ifdef __cplusplus
}
#endif
#endif /* mkv_writer_h */
