#ifndef _mpeg_ts_h_
#define _mpeg_ts_h_

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*mpeg_ts_cbwrite)(void* param, const void* packet, int bytes);

void* mpeg_ts_create(mpeg_ts_cbwrite func, void* param);

int mpeg_ts_destroy(void* ts);

int mpeg_ts_write(void* ts, int streamId, unsigned char* data, int bytes);

#ifdef __cplusplus
}
#endif
#endif /* !_mpeg_ts_h_ */
