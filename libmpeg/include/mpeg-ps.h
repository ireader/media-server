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
	STREAM_AUDIO_EAC3	= 0x87,
	STREAM_AUDIO_G711A	= 0x90,
	STREAM_AUDIO_G711U	= 0x91,
	STREAM_AUDIO_G722	= 0x92,
	STREAM_AUDIO_G723	= 0x93,
	STREAM_AUDIO_G729	= 0x99,
	STREAM_AUDIO_SVAC	= 0x9B,
	STREAM_AUDIO_OPUS   = 0x9C,
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
	/// @return 0-ok, other-error
	int (*write)(void* param, int stream, void* packet, size_t bytes);
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


/// @param[in] codecid 0-unknown, other-enum EPSI_STREAM_TYPE, see more @mpeg-ts-proto.h
/// @return 0-ok, other-error
typedef int (*ps_demuxer_onpacket)(void* param, int stream, int codecid, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes);

struct ps_demuxer_t; 
struct ps_demuxer_t* ps_demuxer_create(ps_demuxer_onpacket onpacket, void* param);
int ps_demuxer_destroy(struct ps_demuxer_t* demuxer);

/// ps_demuxer_input return consumed bytes, the remain data MUST save and merge with next packet
/// int n = ps_demuxer_input(demuxer, data, bytes);
/// if(n >= 0 && n < bytes)
///		memcpy(NEXTBUFFER, data + n, bytes - n);
/// 
/// @return >=0-consume bytes, <0-error
int ps_demuxer_input(struct ps_demuxer_t* demuxer, const uint8_t* data, size_t bytes);

struct ps_demuxer_notify_t
{
	/// @param[in] param ps_demuxer_set_notify param
	/// @param[in] stream ps stream id
	/// @param[in] codecid ps codecid, e.g. STREAM_VIDEO_H264
	/// @param[in] extra stream extra data
	/// @param[in] bytes extra data length
	/// @param[in] finish 0-have more stream, 1-no more streams
	void (*onstream)(void* param, int stream, int codecid, const void* extra, int bytes, int finish);
};

/// Set ps notify on PSM change
void ps_demuxer_set_notify(struct ps_demuxer_t* demuxer, struct ps_demuxer_notify_t* notify, void* param);

#ifdef __cplusplus
}
#endif
#endif /* !_mpeg_ps_h_ */
