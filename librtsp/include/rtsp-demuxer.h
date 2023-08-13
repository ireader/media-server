#ifndef _rtsp_demuxer_h_
#define _rtsp_demuxer_h_

#include <stdint.h>
#include "avpacket.h" // https://github.com/ireader/avcodec #avcodec/include

#if defined(__cplusplus)
extern "C" {
#endif

struct rtsp_demuxer_t;

/// @param[in] pkt rtp packet flags, RTP_PAYLOAD_FLAG_PACKET_xxx, see more @rtp-payload.h
typedef int (*rtsp_demuxer_onpacket)(void* param, struct avpacket_t* pkt);

/// @param[in] stream user defined stream id, copy to rtsp_demuxer_onpacket pkt->stream->stream
/// @param[in] jitter rtp reorder jitter(ms), e.g. 200(ms)
struct rtsp_demuxer_t* rtsp_demuxer_create(int stream, int jitter, rtsp_demuxer_onpacket onpkt, void* param);
int rtsp_demuxer_destroy(struct rtsp_demuxer_t* demuxer);

/// @param[in] frequency audio/video sample rate, e.g. video 90000, audio 48000
/// @param[in] payload rtp payload id, see more @rtp-profile.h
/// @param[in] encoding rtp payload encoding, see more @rtp-profile.h
/// @param[in] fmtp rtp payload format parameter, sdp a=fmtp
/// @return 0-ok, other-error
int rtsp_demuxer_add_payload(struct rtsp_demuxer_t* demuxer, int frequency, int payload, const char* encoding, const char* fmtp);

/// Set RTP-Info
int rtsp_demuxer_rtpinfo(struct rtsp_demuxer_t* demuxer, uint16_t seq, uint32_t timestamp);

/// @return 0-ok, >0-rtcp message type, <0-error
int rtsp_demuxer_input(struct rtsp_demuxer_t* demuxer, const void* data, int bytes);

/// Get RTCP packet
/// @return >0-rtcp report length, 0-don't need send rtcp
int rtsp_demuxer_rtcp(struct rtsp_demuxer_t* demuxer, void* buf, int len);

/// @param[out] lost read lost packets by jitter
/// @param[out] late received after read
/// @param[out] misorder reorder packets
/// @param[out] duplicate exist in unread queue
/// @return 0-ok, other-error
int rtsp_demuxer_stats(struct rtsp_demuxer_t* demuxer, int* lost, int* late, int* misorder, int* duplicate);

#if defined(__cplusplus)
}
#endif
#endif /* _rtsp_demuxer_h_ */
