#ifndef _mp4_writer_h_
#define _mp4_writer_h_

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include "mov-buffer.h"
#include "mov-format.h"
#include "mov-writer.h"
#include "fmp4-writer.h"

#ifdef __cplusplus
extern "C" {
#endif

struct mp4_writer_t 
{
    mov_writer_t *mov;
    fmp4_writer_t* fmp4;
};

/// @param[in] flags mov flags, such as: MOV_FLAG_SEGMENT, see more @mov-format.h
static inline struct mp4_writer_t* mp4_writer_create(int is_fmp4, const struct mov_buffer_t* buffer, void* param, int flags)
{
    struct mp4_writer_t* mp4;
    mp4 = (struct mp4_writer_t*)calloc(1, sizeof(struct mp4_writer_t));
    if (!mp4) return NULL;

    if (!is_fmp4) {
        mp4->mov = mov_writer_create(buffer, param, flags); 
    } else {
        mp4->fmp4 = fmp4_writer_create(buffer, param, flags);
    }
    return mp4;
}

static inline void mp4_writer_destroy(struct mp4_writer_t* mp4)
{
    assert((mp4->fmp4 && !mp4->mov) || (!mp4->fmp4 && mp4->mov));
    if (mp4->mov) {
        mov_writer_destroy(mp4->mov); 
    } else {
        fmp4_writer_destroy(mp4->fmp4);
    }
    free(mp4);
}

/// @param[in] object MPEG-4 systems ObjectTypeIndication such as: MOV_OBJECT_AAC, see more @mov-format.h
/// @param[in] extra_data AudioSpecificConfig
/// @return >=0-track, <0-error
static inline int mp4_writer_add_audio(struct mp4_writer_t* mp4, uint8_t object, int channel_count, int bits_per_sample, int sample_rate, const void* extra_data, size_t extra_data_size)
{
    assert((mp4->fmp4 && !mp4->mov) || (!mp4->fmp4 && mp4->mov));
    if (mp4->mov) {
        return mov_writer_add_audio(mp4->mov, object, channel_count, bits_per_sample, sample_rate, extra_data, extra_data_size);
    } else {
        return fmp4_writer_add_audio(mp4->fmp4, object, channel_count, bits_per_sample, sample_rate, extra_data, extra_data_size);
    }
}

/// @param[in] object MPEG-4 systems ObjectTypeIndication such as: MOV_OBJECT_H264, see more @mov-format.h
/// @param[in] extra_data AVCDecoderConfigurationRecord/HEVCDecoderConfigurationRecord
/// @return >=0-track, <0-error
static inline int mp4_writer_add_video(struct mp4_writer_t* mp4, uint8_t object, int width, int height, const void* extra_data, size_t extra_data_size)
{
    assert((mp4->fmp4 && !mp4->mov) || (!mp4->fmp4 && mp4->mov));
    if (mp4->mov) {
        return mov_writer_add_video(mp4->mov, object, width, height, extra_data, extra_data_size);
    } else {
        return fmp4_writer_add_video(mp4->fmp4, object, width, height, extra_data, extra_data_size);
    }
}

static inline int mp4_writer_add_subtitle(struct mp4_writer_t* mp4, uint8_t object, const void* extra_data, size_t extra_data_size)
{
    assert((mp4->fmp4 && !mp4->mov) || (!mp4->fmp4 && mp4->mov));
    if (mp4->mov) {
        return mov_writer_add_subtitle(mp4->mov, object, extra_data, extra_data_size);
    } else {
        return fmp4_writer_add_subtitle(mp4->fmp4, object, extra_data, extra_data_size);
    }
}

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
static inline int mp4_writer_write(struct mp4_writer_t* mp4, int track, const void* data, size_t bytes, int64_t pts, int64_t dts, int flags)
{
    assert((mp4->fmp4 && !mp4->mov) || (!mp4->fmp4 && mp4->mov));
    if (mp4->mov) {
        return mov_writer_write(mp4->mov, track, data, bytes, pts, dts, flags);
    } else {
        return fmp4_writer_write(mp4->fmp4, track, data, bytes, pts, dts, flags);
    }
}

///////////////////// The following interfaces are only applicable to fmp4 ///////////////////////////////

/// Save data and open next segment
/// @return 0-ok, other-error
static inline int mp4_writer_save_segment(struct mp4_writer_t* mp4)
{
    assert((mp4->fmp4 && !mp4->mov) || (!mp4->fmp4 && mp4->mov));
    if (mp4->fmp4)
        return fmp4_writer_save_segment(mp4->fmp4);
    return 0;
}

/// Get init segment data(write FTYP, MOOV only)
/// WARNING: it caller duty to switch file/buffer context with fmp4_writer_write
/// @return 0-ok, other-error
static inline int mp4_writer_init_segment(struct mp4_writer_t* mp4)
{
    assert((mp4->fmp4 && !mp4->mov) || (!mp4->fmp4 && mp4->mov));
    if (mp4->fmp4)
        return fmp4_writer_init_segment(mp4->fmp4);
    return 0;
}

#ifdef __cplusplus
}
#endif
#endif /* !_mp4_writer_h_ */
