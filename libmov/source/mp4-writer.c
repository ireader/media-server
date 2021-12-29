#include <assert.h>
#include <stdlib.h>
#include "fmp4-writer.h"
#include "mov-writer.h"
#include "mp4-writer.h"

struct mp4_writer_t {
    int is_fmp4;
    union {
        fmp4_writer_t *fmp4;
        mov_writer_t *mov;
    } u;
};

struct mp4_writer_t *mp4_writer_create(int is_fmp4, const struct mov_buffer_t *buffer, void *param, int flags) {
    struct mp4_writer_t *mp4 = (struct mp4_writer_t *) malloc(sizeof(struct mp4_writer_t));
    assert(mp4);
    mp4->is_fmp4 = is_fmp4;
    if (is_fmp4) {
        mp4->u.fmp4 = fmp4_writer_create(buffer, param, flags);
    } else {
        mp4->u.mov = mov_writer_create(buffer, param, flags);
    }
    return mp4;
}

void mp4_writer_destroy(struct mp4_writer_t *mp4) {
    assert(mp4);
    if (mp4->is_fmp4) {
        fmp4_writer_destroy(mp4->u.fmp4);
    } else {
        mov_writer_destroy(mp4->u.mov);
    }
    free(mp4);
}

int mp4_writer_add_audio(struct mp4_writer_t *mp4, uint8_t object, int channel_count, int bits_per_sample, int sample_rate, const void *extra_data, size_t extra_data_size) {
    assert(mp4);
    if (mp4->is_fmp4) {
        return fmp4_writer_add_audio(mp4->u.fmp4, object, channel_count, bits_per_sample, sample_rate, extra_data, extra_data_size);
    } else {
        return mov_writer_add_audio(mp4->u.mov, object, channel_count, bits_per_sample, sample_rate, extra_data, extra_data_size);
    }
}

int mp4_writer_add_video(struct mp4_writer_t *mp4, uint8_t object, int width, int height, const void *extra_data, size_t extra_data_size) {
    assert(mp4);
    if (mp4->is_fmp4) {
        return fmp4_writer_add_video(mp4->u.fmp4, object, width, height, extra_data, extra_data_size);
    } else {
        return mov_writer_add_video(mp4->u.mov, object, width, height, extra_data, extra_data_size);
    }
}

int mp4_writer_add_subtitle(struct mp4_writer_t *mp4, uint8_t object, const void *extra_data, size_t extra_data_size) {
    assert(mp4);
    if (mp4->is_fmp4) {
        return fmp4_writer_add_subtitle(mp4->u.fmp4, object, extra_data, extra_data_size);
    } else {
        return mov_writer_add_subtitle(mp4->u.mov, object, extra_data, extra_data_size);
    }
}

int mp4_writer_write(struct mp4_writer_t *mp4, int track, const void *data, size_t bytes, int64_t pts, int64_t dts, int flags) {
    assert(mp4);
    if (mp4->is_fmp4) {
        return fmp4_writer_write(mp4->u.fmp4, track, data, bytes, pts, dts, flags);
    } else {
        return mov_writer_write(mp4->u.mov, track, data, bytes, pts, dts, flags);
    }
}

int mp4_writer_save_segment(struct mp4_writer_t *mp4) {
    assert(mp4);
    if (mp4->is_fmp4) {
        return fmp4_writer_save_segment(mp4->u.fmp4);
    } else {
        return -1;
    }
}

int mp4_writer_init_segment(struct mp4_writer_t *mp4) {
    assert(mp4);
    if (mp4->is_fmp4) {
        return fmp4_writer_init_segment(mp4->u.fmp4);
    } else {
        return -1;
    }
}