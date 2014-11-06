#ifndef _rtp_member_h_
#define _rtp_member_h_

#include "time64.h"
#include "ctypedef.h"
#include "rtcp-header.h"
#include <stdlib.h>

struct rtp_member
{
	int32_t ref;

	uint32_t ssrc;					// ssrc == rtcp_sr.ssrc == rtcp_rb.ssrc
	rtcp_sr_t rtcp_sr;
	rtcp_rb_t rtcp_rb;
	rtcp_sdes_item_t sdes[9];		// SDES item
	time64_t rtcp_rr_clock;			// last RTCP RR packet received time	
	time64_t rtcp_sr_clock;			// last RTCP SR packet received time	

	time64_t rtp_clock;				// last send/received RTP packet time
	uint32_t rtp_timestamp;			// last send/received RTP packet RTP timestamp(in packet header)
	uint32_t rtp_packets;			// send/received RTP packet count
	uint32_t rtp_octets;			// send/received RTP octet count

	double jitter;

	uint16_t seq_base;				// max sequence number
	uint16_t seq_max;				// max sequence number
	uint16_t seq_cycles;			// high extension sequence number
	uint32_t rtp_expected;			// previous SR/RR expect RTP sequence number
	uint32_t rtp_received;			// previous SR/RR RTP packet count
};

struct rtp_member* rtp_member_create(uint32_t ssrc);
void rtp_member_addref(struct rtp_member *member);
void rtp_member_release(struct rtp_member *member);

int rtp_member_setvalue(struct rtp_member *member, int item, const unsigned char* data, size_t bytes);

#endif /* !_rtp_member_h_ */
