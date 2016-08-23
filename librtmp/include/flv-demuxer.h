#ifndef _flv_demuxer_h_
#define _flv_demuxer_h_

#include <inttypes.h>
#include <stdint.h>
#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif
	enum { FLV_AAC = 1, FLV_AVC, FLV_AAC_HEADER, FLV_AVC_HEADER };

	typedef void (*flv_demuxer_handler)(void* param, int type, const void* data, size_t bytes, uint32_t pts, uint32_t dts);

	void* flv_demuxer_create(flv_demuxer_handler handler, void* param);
	void flv_demuxer_destroy(void* flv);

	size_t flv_demuxer_input(void* flv, const void* data, size_t bytes);

#if defined(__cplusplus)
}
#endif
#endif /* !_flv_demuxer_h_ */
