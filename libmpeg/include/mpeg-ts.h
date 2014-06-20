#ifndef _mpeg_ts_h_
#define _mpeg_ts_h_

#ifdef __cplusplus
extern "C" {
#endif

typedef __int64 int64_t;

typedef void (*mpeg_ts_cbwrite)(void* param, const void* packet, int bytes);

void* mpeg_ts_create(mpeg_ts_cbwrite func, void* param);

int mpeg_ts_destroy(void* ts);

int mpeg_ts_write(void* ts, int streamId, int64_t pcr, int64_t pts, int64_t dts, const void* data, int bytes);

#ifdef __cplusplus
}
#endif
#endif /* !_mpeg_ts_h_ */
