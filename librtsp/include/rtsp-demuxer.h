#ifndef _rtsp_demuxer_h_
#define _rtsp_demuxer_h_

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

struct rtsp_demuxer_t;

/// @param[in] track PS/TS track id, 0 for other
/// @param[in] flags rtp packet flags, RTP_PAYLOAD_FLAG_PACKET_xxx, see more @rtp-payload.h
typedef int (*rtsp_demuxer_onpacket)(void* param, int track, int payload, const char* encoding, int64_t pts, int64_t dts, const void* data, int bytes, int flags);

/// @param[in] jitter rtp reorder jitter(ms), e.g. 200(ms)
/// @param[in] frequency audio/video sample rate, e.g. video 90000, audio 48000
/// @param[in] payload rtp payload id, see more @rtp-profile.h
/// @param[in] encoding rtp payload encoding, see more @rtp-profile.h
struct rtsp_demuxer_t* rtsp_demuxer_create(int jitter, int frequency, int payload, const char* encoding, const char* fmtp, rtsp_demuxer_onpacket onpkt, void* param);
int rtsp_demuxer_destroy(struct rtsp_demuxer_t* demuxer);

/// @param[in] frequency audio/video sample rate, e.g. video 90000, audio 48000
/// @param[in] payload rtp payload id, see more @rtp-profile.h
/// @param[in] encoding rtp payload encoding, see more @rtp-profile.h
/// @return 0-ok, other-error
int rtsp_demuxer_add_payload(struct rtsp_demuxer_t* demuxer, int frequency, int payload, const char* encoding, const char* fmtp);

/// Set RTP-Info
int rtsp_demuxer_rtpinfo(struct rtsp_demuxer_t* demuxer, uint16_t seq, uint32_t timestamp);

/// @return 0-ok, >0-rtcp message type, <0-error
int rtsp_demuxer_input(struct rtsp_demuxer_t* demuxer, const void* data, int bytes);

/// Get RTCP packet
/// @return >0-rtcp report length, 0-don't need send rtcp
int rtsp_demuxer_rtcp(struct rtsp_demuxer_t* demuxer, void* buf, int len);

#if defined(__cplusplus)
}
#endif
#endif /* _rtsp_demuxer_h_ */
