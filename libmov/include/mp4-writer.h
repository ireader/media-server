#ifndef _mp4_writer_h_
#define _mp4_writer_h_

#include <stddef.h>
#include <stdint.h>
#include "mov-buffer.h"
#include "mov-format.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mp4_writer_t mp4_writer_t;

/// @param[in] flags mov flags, such as: MOV_FLAG_SEGMENT, see more @mov-format.h
mp4_writer_t* mp4_writer_create(int is_fmp4, const struct mov_buffer_t *buffer, void* param, int flags);
void mp4_writer_destroy(mp4_writer_t* mp4);

/// @param[in] object MPEG-4 systems ObjectTypeIndication such as: MOV_OBJECT_H264, see more @mov-format.h
/// @param[in] extra_data AudioSpecificConfig/AVCDecoderConfigurationRecord/HEVCDecoderConfigurationRecord
/// @return >=0-track, <0-error
int mp4_writer_add_audio(mp4_writer_t* mp4, uint8_t object, int channel_count, int bits_per_sample, int sample_rate, const void* extra_data, size_t extra_data_size);
int mp4_writer_add_video(mp4_writer_t* mp4, uint8_t object, int width, int height, const void* extra_data, size_t extra_data_size);
int mp4_writer_add_subtitle(mp4_writer_t* mp4, uint8_t object, const void* extra_data, size_t extra_data_size);

/// Write audio/video stream
/// raw AAC data, don't include ADTS/AudioSpecificConfig
/// H.264/H.265 MP4 format, replace start code(0x00000001) with NALU size
/// @param[in] track return by mov_writer_add_audio/mov_writer_add_video
/// @param[in] data audio/video frame
/// @param[in] bytes buffer size
/// @param[in] pts timestamp in millisecond
/// @param[in] dts timestamp in millisecond
/// @param[in] flags MOV_AV_FLAG_XXX, such as: MOV_AV_FLAG_KEYFREAME, see more @mov-format.h
/// @return 0-ok, other-error
int mp4_writer_write(mp4_writer_t* mp4, int track, const void* data, size_t bytes, int64_t pts, int64_t dts, int flags);

///////////////////// The following interfaces are only applicable to fmp4 ///////////////////////////////

/// Save data and open next segment
/// @return 0-ok, other-error
int mp4_writer_save_segment(mp4_writer_t* mp4);

/// Get init segment data(write FTYP, MOOV only)
/// WARNING: it caller duty to switch file/buffer context with fmp4_writer_write
/// @return 0-ok, other-error
int mp4_writer_init_segment(mp4_writer_t* mp4);

#ifdef __cplusplus
}
#endif
#endif /* !_mp4_writer_h_ */
