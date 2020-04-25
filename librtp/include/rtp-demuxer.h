#ifndef _rtp_demuxer_h_
#define _rtp_demuxer_h_

#include <stdint.h>

struct rtp_demuxer_t;

typedef void (*rtp_demuxer_onpacket)(void* param, const void *packet, int bytes, uint32_t timestamp, int flags);

struct rtp_demuxer_t* rtp_demuxer_create(int frequency, int payload, const char* encoding, rtp_demuxer_onpacket onpkt, void* param);
int rtp_demuxer_destroy(struct rtp_demuxer_t** rtp);

/// @param[in] data a rtp/rtcp packet
/// @return >0-rtcp message, 0-ok, <0-error
int rtp_demuxer_input(struct rtp_demuxer_t* rtp, const void* data, int bytes);

/// @return >0-rtcp report length, 0-don't need send rtcp
int rtp_demuxer_rtcp(struct rtp_demuxer_t* rtp, void* buf, int len);

#endif /* _rtp_demuxer_h_ */
