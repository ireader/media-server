#ifndef _mpeg_ts_h_
#define _mpeg_ts_h_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct mpeg_ts_func_t
{
	/// alloc new packet
	/// @param[in] param use-defined parameter(by mpeg_ps_create)
	/// @param[in] bytes alloc memory size in byte(default 188)
	/// @return memory pointer
	void* (*alloc)(void* param, size_t bytes);

	/// free packet
	/// @param[in] param use-defined parameter(by mpeg_ps_create)
	/// @param[in] packet PS packet pointer(alloc return pointer)
	void (*free)(void* param, void* packet);

	/// callback on PS packet done
	/// @param[in] param use-defined parameter(by mpeg_ps_create)
	/// @param[in] packet PS packet pointer(alloc return pointer)
	/// @param[in] bytes packet size
	void (*write)(void* param, const void* packet, size_t bytes);
};

void* mpeg_ts_create(const struct mpeg_ts_func_t *func, void* param);

int mpeg_ts_destroy(void* ts);

int mpeg_ts_write(void* ts, int streamId, int64_t pts, int64_t dts, const void* data, size_t bytes);

int mpeg_ts_reset(void* ts);

typedef void (*onpacket)(void* param, int avtype, int64_t pts, int64_t dts, void* data, size_t bytes);
int mpeg_ts_packet_dec(const unsigned char* data, size_t bytes, onpacket handler, void* param);

#ifdef __cplusplus
}
#endif
#endif /* !_mpeg_ts_h_ */
