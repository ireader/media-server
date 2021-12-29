#include <assert.h>
#include <stdlib.h>
#include "mpeg-muxer.h"

struct mpeg_muxer_t {
    int is_ps;
    union {
        struct {
            void *ctx;
            void *param;
            mpeg_muxer_func_t func;
        } ts;
        struct ps_muxer_t *ps;
    } u;
};

static void *on_mpeg_ts_alloc(void *param, size_t bytes) {
    mpeg_muxer_t *mpeg = (mpeg_muxer_t *) param;
    return mpeg->u.ts.func.alloc(mpeg->u.ts.param, bytes);
}

static void on_mpeg_ts_free(void *param, void *packet) {
    mpeg_muxer_t *mpeg = (mpeg_muxer_t *) param;
    mpeg->u.ts.func.free(mpeg->u.ts.param, packet);
}

static int on_mpeg_ts_write(void *param, const void *packet, size_t bytes) {
    mpeg_muxer_t *mpeg = (mpeg_muxer_t *) param;
    return mpeg->u.ts.func.write(mpeg->u.ts.param, 0, (void *) packet, bytes);
}

mpeg_muxer_t *mpeg_muxer_create(int is_ps, const mpeg_muxer_func_t *func, void *param) {
    mpeg_muxer_t *mpeg = (mpeg_muxer_t *) malloc(sizeof(mpeg_muxer_t));
    assert(mpeg);
    mpeg->is_ps = is_ps;
    if (is_ps) {
        mpeg->u.ps = ps_muxer_create(func, param);
    } else {
        struct mpeg_ts_func_t ts_func = {on_mpeg_ts_alloc, on_mpeg_ts_free, on_mpeg_ts_write};
        mpeg->u.ts.func = *func;
        mpeg->u.ts.param = param;
        mpeg->u.ts.ctx = mpeg_ts_create(&ts_func, mpeg);
    }
    return mpeg;
}

int mpeg_muxer_destroy(mpeg_muxer_t *muxer) {
    assert(muxer);
    int ret = -1;
    if (muxer->is_ps) {
        ret = ps_muxer_destroy(muxer->u.ps);
    } else {
        ret = mpeg_ts_destroy(muxer->u.ts.ctx);
    }
    free(muxer);
    return ret;
}

int mpeg_muxer_add_stream(mpeg_muxer_t *muxer, int codecid, const void *extradata, size_t extradata_size) {
    assert(muxer);
    if (muxer->is_ps) {
        return ps_muxer_add_stream(muxer->u.ps, codecid, extradata, extradata_size);
    }
    return mpeg_ts_add_stream(muxer->u.ts.ctx, codecid, extradata, extradata_size);
}

int mpeg_muxer_input(mpeg_muxer_t *muxer, int stream, int flags, int64_t pts, int64_t dts, const void *data, size_t bytes) {
    assert(muxer);
    if (muxer->is_ps) {
        return ps_muxer_input(muxer->u.ps, stream, flags, pts, dts, data, bytes);
    }
    return mpeg_ts_write(muxer->u.ts.ctx, stream, flags, pts, dts, data, bytes);
}

int mpeg_muxer_reset(mpeg_muxer_t *muxer) {
    assert(muxer);
    if (muxer->is_ps) {
        return -1;
    }
    return mpeg_ts_reset(muxer->u.ts.ctx);
}

int mpeg_muxer_add_program(mpeg_muxer_t *muxer, uint16_t pn, const void *info, int bytes) {
    assert(muxer);
    if (muxer->is_ps) {
        return -1;
    }
    return mpeg_ts_add_program(muxer->u.ts.ctx, pn, info, bytes);
}

int mpeg_muxer_remove_program(mpeg_muxer_t *muxer, uint16_t pn) {
    assert(muxer);
    if (muxer->is_ps) {
        return -1;
    }
    return mpeg_ts_remove_program(muxer->u.ts.ctx, pn);
}

int mpeg_muxer_add_program_stream(mpeg_muxer_t *muxer, uint16_t pn, int codecid, const void *extra_data, size_t extra_data_size) {
    assert(muxer);
    if (muxer->is_ps) {
        return -1;
    }
    return mpeg_ts_add_program_stream(muxer->u.ts.ctx, pn, codecid, extra_data, extra_data_size);
}