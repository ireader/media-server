#ifndef _rtp_h264_unpack_h_
#define _rtp_h264_unpack_h_

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*rtp_h264_unpack_onnal)(void* param, unsigned char nal, const void* data, int bytes);

void* rtp_h264_unpack_create(rtp_h264_unpack_onnal callback, void* param);
void rtp_h264_unpack_destroy(void* unpacker);

/// H.264 pack
/// @param[in] unpacker create by rtp_h264_unpack_create
/// @param[in] packet RTP packet
/// @param[in] bytes RTP packet length in bytes
/// @return 0-ok, <0-failed
int rtp_h264_unpack_input(void* unpacker, const void* packet, size_t bytes);

#ifdef __cplusplus
}
#endif
#endif /* !_rtp_h264_unpack_h_ */
