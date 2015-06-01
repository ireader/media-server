#ifndef _rtp_unpack_h_
#define _rtp_unpack_h_

#ifdef __cplusplus
extern "C" {
#endif

struct rtp_unpack_func_t
{
//	void* (*alloc)(void* param, size_t bytes);
//	void (*free)(void* param, void *packet);
	void (*packet)(void* param, unsigned char streamid, const void *packet, size_t bytes, uint64_t time);
};

struct rtp_unpack_t
{
	void* (*create)(struct rtp_unpack_func_t *func, void* param);
	void (*destroy)(void* packer);

	/// RTP packet to PS/H.264 Elementary Stream
	/// @param[in] unpacker
	/// @param[in] packet RTP packet
	/// @param[in] bytes RTP packet length in bytes
	/// @param[in] time stream UTC time
	/// @return 0-ok, <0-failed
	int (*input)(void* packer, const void* packet, size_t bytes, uint64_t time);
};

struct rtp_unpack_t *rtp_ps_unpacker();
struct rtp_unpack_t *rtp_h264_unpacker();

#ifdef __cplusplus
}
#endif
#endif /* !_rtp_unpack_h_ */
