#ifndef _rtp_demuxer_h_
#define _rtp_demuxer_h_

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

struct rtp_demuxer_t;

/// @param[in] param rtp_demuxer_create input param
/// @param[in] packet rtp payload data
/// @param[in] bytes rtp payload length in byte
/// @param[in] timestamp rtp timestamp(relation at sample rate)
/// @param[in] flags rtp packet flags, RTP_PAYLOAD_FLAG_PACKET_xxx, see more @rtp-payload.h
/// @return 0-ok, other-error
typedef int (*rtp_demuxer_onpacket)(void* param, const void *packet, int bytes, uint32_t timestamp, int flags);

/// @param[in] jitter rtp reorder jitter(ms), e.g. 200(ms)
/// @param[in] frequency audio/video sample rate, e.g. video 90000, audio 48000
/// @param[in] payload rtp payload id, see more @rtp-profile.h
/// @param[in] encoding rtp payload encoding, see more @rtp-profile.h
struct rtp_demuxer_t* rtp_demuxer_create(int jitter, int frequency, int payload, const char* encoding, rtp_demuxer_onpacket onpkt, void* param);
int rtp_demuxer_destroy(struct rtp_demuxer_t** rtp);

/// @param[in] data a rtp/rtcp packet
/// @return >0-rtcp message, 0-ok, <0-error
int rtp_demuxer_input(struct rtp_demuxer_t* rtp, const void* data, int bytes);

/// @return >0-rtcp report length, 0-don't need send rtcp
int rtp_demuxer_rtcp(struct rtp_demuxer_t* rtp, void* buf, int len);

/// @param[out] lost read lost packets by jitter
/// @param[out] late received after read
/// @param[out] misorder reorder packets
/// @param[out] duplicate exist in unread queue
void rtp_demuxer_stats(struct rtp_demuxer_t* rtp, int* lost, int* late, int* misorder, int* duplicate);

#if defined(__cplusplus)
}
#endif
#endif /* _rtp_demuxer_h_ */
