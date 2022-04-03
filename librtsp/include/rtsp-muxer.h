#ifndef _rtsp_muxer_h_
#define _rtsp_muxer_h_

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

struct rtsp_muxer_t;

/// @param[in] flags 0x0100-packet lost, 0x0200-packet corrupt
/// @return 0-ok, other-error
typedef int (*rtsp_muxer_onpacket)(void* param, int pid, const void* data, int bytes, uint32_t timestamp, int flags);

struct rtsp_muxer_t* rtsp_muxer_create(rtsp_muxer_onpacket onpacket, void* param);
int rtsp_muxer_destroy(struct rtsp_muxer_t* muxer);

/// @param[in] proto RTP/AVP, see more @librtsp/include/sdp-options.h
/// @param[in] payload rtp payload id, e.g. RTP_PAYLOAD_MP2P, see more @librtp/include/rtp-profile.h
/// @param[in] encoding rtp payload encoding(for payload > 96 only)
/// @return >=0-paylad index, <0-error
int rtsp_muxer_add_payload(struct rtsp_muxer_t* muxer, const char* proto, int frequence, int payload, const char* encoding, uint16_t seq, uint32_t ssrc, uint16_t port, const void* extra, int size);

/// @param[in] pid payload index, create by rtsp_muxer_add_payload
/// @param[in] codec rtp media codec id, e.g. RTP_PAYLOAD_H264, see more @librtp/include/rtp-profile.h
/// @return >=0-media index (for rtsp_muxer_input), <0-error
int rtsp_muxer_add_media(struct rtsp_muxer_t* muxer, int pid, int codec, const void* extra, int size);

/// Get RTP-Info
/// @return 0-ok, <0-error
int rtsp_muxer_getinfo(struct rtsp_muxer_t* muxer, int pid, uint16_t* seq, uint32_t* timestamp, const char** sdp, int *size);

/// @param[in] mid media index, create by rtsp_muxer_add_media
/// @return 0-ok, >0-rtcp message type, <0-error
int rtsp_muxer_input(struct rtsp_muxer_t* muxer, int mid, int64_t pts, int64_t dts, const void* data, int bytes, int flags);

/// Get RTCP packet
/// @return >0-rtcp report length, 0-don't need send rtcp
int rtsp_muxer_rtcp(struct rtsp_muxer_t* muxer, int pid, void* buf, int len);

/// Input RTCP packet
int rtsp_muxer_onrtcp(struct rtsp_muxer_t* muxer, int pid, const void* buf, int len);

#if defined(__cplusplus)
}
#endif
#endif /* !_rtsp_muxer_h_ */
