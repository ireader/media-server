#ifndef _mpeg_muxer_h_
#define _mpeg_muxer_h_

#include <stdint.h>
#include <stddef.h>
#include "mpeg-ps.h"
#include "mpeg-ts.h"
#include "mpeg-ts-proto.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mpeg_muxer_t mpeg_muxer_t;
typedef struct ps_muxer_func_t mpeg_muxer_func_t;

/// Create/Destroy MPEG2-TS/PS muxer
mpeg_muxer_t *mpeg_muxer_create(int is_ps, const mpeg_muxer_func_t *func, void *param);
int mpeg_muxer_destroy(mpeg_muxer_t *muxer);

/// Add audio/video stream
/// @param[in] codecid PSI_STREAM_H264/PSI_STREAM_H265/PSI_STREAM_AAC, see more @mpeg-ts-proto.h
/// @param[in] extradata itu h.222.0 program and program element descriptors, NULL for H.264/H.265/AAC
/// @param[in] bytes extradata size in byte
/// @return <=0-error, >0-audio/video stream id
int mpeg_muxer_add_stream(mpeg_muxer_t *muxer, int codecid, const void *extradata, size_t extradata_size);

/// input ES
/// @param[in] muxer object return by mpeg_muxer_create
/// @param[in] stream stream id, return by mpeg_muxer_add_stream
/// @param[in] flags 0x0001-video IDR frame, 0x8000-H.264/H.265 with AUD
/// @param[in] pts presentation time stamp(in 90KHZ)
/// @param[in] dts decoding time stamp(in 90KHZ)
/// @param[in] data ES memory
/// @param[in] bytes ES length in byte
/// @return 0-ok, ENOMEM-alloc failed, <0-error
int mpeg_muxer_input(mpeg_muxer_t *muxer, int stream, int flags, int64_t pts, int64_t dts, const void *data, size_t bytes);

///////////////////// The following interfaces are only applicable to mpeg-ts ///////////////////////////////

/// Reset PAT/PCR period
int mpeg_muxer_reset(mpeg_muxer_t *muxer);

/// FOR MULTI-PROGRAM TS STREAM ONLY
/// Add a program
/// @param[in] pn program number, valid value: [1, 0xFFFF]
/// @return 0-OK, <0-error
int mpeg_muxer_add_program(mpeg_muxer_t *muxer, uint16_t pn, const void *info, int bytes);

/// Remove a program by program number
/// @param[in] pn program number, valid value: [1, 0xFFFF]
/// @return 0-OK, <0-error
int mpeg_muxer_remove_program(mpeg_muxer_t *muxer, uint16_t pn);

/// Add program stream(same as mpeg_ts_add_stream except program number)
/// @param[in] pn mpeg_ts_add_program program number
/// @return 0-OK, <0-error
int mpeg_muxer_add_program_stream(mpeg_muxer_t *muxer, uint16_t pn, int codecid, const void *extra_data, size_t extra_data_size);

#ifdef __cplusplus
}
#endif
#endif /* !_mpeg_muxer_h_ */
