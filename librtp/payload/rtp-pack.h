#ifndef _rtp_pack_h_
#define _rtp_pack_h_

#ifdef __cplusplus
extern "C" {
#endif

struct rtp_pack_func_t
{
	void* (*alloc)(void* param, size_t bytes);
	void (*free)(void* param, void *packet);
	void (*packet)(void* param, void *packet, size_t bytes, int64_t time);
};

struct rtp_pack_t
{
	/// create RTP packer
	/// @param[in] ssrc RTP header SSRC filed
	/// @param[in] seq RTP header sequence number filed
	/// @param[in] payload RTP header PT filed (see more about rtp-profile.h)
	/// @param[in] func user-defined callback
	/// @param[in] param user-defined parameter
	/// @return RTP packer
	void* (*create)(uint8_t payload, uint16_t seq, uint32_t ssrc, uint32_t frequency, struct rtp_pack_func_t *func, void* param);
	/// destroy RTP Packer
	void (*destroy)(void* packer);

	void (*get_info)(void* packer, uint16_t* seq, uint32_t* timestamp);

	/// PS/H.264 Elementary Stream to RTP Packet
	/// @param[in] packer
	/// @param[in] data stream data
	/// @param[in] bytes stream length in bytes
	/// @param[in] time stream UTC time
	/// @return 0-ok, ENOMEM-alloc failed, <0-failed
	int (*input)(void* packer, const void* data, size_t bytes, int64_t time);
};

struct rtp_pack_t *rtp_packer();
struct rtp_pack_t *rtp_ps_packer();
struct rtp_pack_t *rtp_ts_packer();
struct rtp_pack_t *rtp_h264_packer();

void rtp_pack_setsize(size_t bytes);
size_t rtp_pack_getsize();

#ifdef __cplusplus
}
#endif
#endif /* !_rtp_pack_h_ */
