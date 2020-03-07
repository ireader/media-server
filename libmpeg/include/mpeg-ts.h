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

/// Create/Destroy MPEG2-TS muxer
void* mpeg_ts_create(const struct mpeg_ts_func_t *func, void* param);
int mpeg_ts_destroy(void* ts);

/// Add audio/video stream
/// @param[in] codecid PSI_STREAM_H264/PSI_STREAM_H265/PSI_STREAM_AAC, see more @mpeg-ts-proto.h
/// @param[in] extradata itu h.222.0 program and program element descriptors, NULL for H.264/H.265/AAC
/// @param[in] extradata_size extradata size in byte
/// @return <=0-error, >0-audio/video stream id
int mpeg_ts_add_stream(void* ts, int codecid, const void* extradata, size_t extradata_size);

/// Muxer audio/video stream data
/// @param[in] stream stream id by mpeg_ts_add_stream
/// @param[in] flags 0x0001-video IDR frame, 0x8000-H.264/H.265 with AUD
/// @param[in] pts audio/video stream timestamp in 90*ms
/// @param[in] dts audio/video stream timestamp in 90*ms
/// @param[in] data H.264/H.265-AnnexB stream(include 00 00 00 01), AAC-ADTS stream
/// @return 0-ok, other-error
int mpeg_ts_write(void* ts, int stream, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes);

/// Reset PAT/PCR period
int mpeg_ts_reset(void* ts);


/// see more mpeg_ts_write
typedef int (*ts_demuxer_onpacket)(void* param, int program, int stream, int codecid, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes);

struct ts_demuxer_t;
struct ts_demuxer_t* ts_demuxer_create(ts_demuxer_onpacket onpacket, void* param);
int ts_demuxer_destroy(struct ts_demuxer_t* demuxer);
size_t ts_demuxer_input(struct ts_demuxer_t* demuxer, const uint8_t* data, size_t bytes);
size_t ts_demuxer_flush(struct ts_demuxer_t* demuxer);
int ts_demuxer_getservice(struct ts_demuxer_t* demuxer, int program, char* provider, int nprovider, char* name, int nname);

#ifdef __cplusplus
}
#endif
#endif /* !_mpeg_ts_h_ */
