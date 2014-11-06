#ifndef _rtp_h264_pack_h_
#define _rtp_h264_pack_h_

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*rtp_h264_pack_onrtp)(void* param, const void* packet);

void* rtp_h264_pack_create(rtp_h264_pack_onrtp callback, void* param);
void rtp_h264_pack_destroy(void* packer);

/// H.264 pack
/// @param[in] packer
/// @param[in] h264 H.264 stream
/// @param[in] bytes H.264 stream length in bytes
/// @return 0-ok, <0-failed
int rtp_h264_pack_input(void* packer, const void* h264, size_t bytes);

void rtp_h264_pack_set_size(size_t max_packet_bytes);

size_t rtp_h264_pack_get_size();

#ifdef __cplusplus
}
#endif
#endif /* !_rtp_h264_pack_h_ */
