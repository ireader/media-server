#ifndef _mpeg_ps_h_
#define _mpeg_ps_h_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum
{
	STREAM_VIDEO_MPEG4	= 0x10,
	STREAM_VIDEO_H264	= 0x1b,
	STREAM_VIDEO_H265   = 0x24,
	STREAM_VIDEO_SVAC	= 0x80,
	STREAM_AUDIO_MP3	= 0x04,
	STREAM_AUDIO_AAC	= 0x0f,
	STREAM_AUDIO_G711	= 0x90,
	STREAM_AUDIO_G722	= 0x92,
	STREAM_AUDIO_G723	= 0x93,
	STREAM_AUDIO_G729	= 0x99,
	STREAM_AUDIO_SVAC	= 0x9B,
};

struct ps_muxer_func_t
{
	/// alloc new packet
	/// @param[in] param user-defined parameter(by ps_muxer_create)
	/// @param[in] bytes alloc memory size in byte
	/// @return memory pointer
	void* (*alloc)(void* param, size_t bytes);

	/// free packet
	/// @param[in] param user-defined parameter(by ps_muxer_create)
	/// @param[in] packet PS packet pointer(alloc return pointer)
	void (*free)(void* param, void* packet);

	/// callback on PS packet done
	/// @param[in] param user-defined parameter(by ps_muxer_create)
	/// @param[in] stream stream id, return by ps_muxer_add_stream
	/// @param[in] packet PS packet pointer(alloc return pointer)
	/// @param[in] bytes packet size
	void (*write)(void* param, int stream, void* packet, size_t bytes);
};

struct ps_muxer_t;
struct ps_muxer_t* ps_muxer_create(const struct ps_muxer_func_t *func, void* param);
int ps_muxer_destroy(struct ps_muxer_t* muxer);
/// Add audio/video stream
/// @param[in] codecid PSI_STREAM_H264/PSI_STREAM_H265/PSI_STREAM_AAC, see more @mpeg-ts-proto.h
/// @param[in] extradata itu h.222.0 program and program element descriptors, NULL for H.264/H.265/AAC
/// @param[in] bytes extradata size in byte
/// @return <=0-error, >0-audio/video stream id
int ps_muxer_add_stream(struct ps_muxer_t* muxer, int codecid, const void* extradata, size_t bytes);

/// input ES
/// @param[in] muxer MPEG-2 Program Stream packer(ps_muxer_create)
/// @param[in] stream stream id, return by ps_muxer_add_stream
/// @param[in] flags 0x0001-video IDR frame, 0x8000-H.264/H.265 with AUD
/// @param[in] pts presentation time stamp(in 90KHZ)
/// @param[in] dts decoding time stamp(in 90KHZ)
/// @param[in] data ES memory
/// @param[in] bytes ES length in byte
/// @return 0-ok, ENOMEM-alloc failed, <0-error
int ps_muxer_input(struct ps_muxer_t* muxer, int stream, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes);


typedef void (*ps_demuxer_onpacket)(void* param, int stream, int codecid, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes);

struct ps_demuxer_t; 
struct ps_demuxer_t* ps_demuxer_create(ps_demuxer_onpacket onpacket, void* param);
int ps_demuxer_destroy(struct ps_demuxer_t* demuxer);
size_t ps_demuxer_input(struct ps_demuxer_t* demuxer, const uint8_t* data, size_t bytes);

#ifdef __cplusplus
}
#endif
#endif /* !_mpeg_ps_h_ */
