#ifndef _rtp_packet_h_
#define _rtp_packet_h_

#include "rtp-header.h"

#define RTP_FIXED_HEADER 12

struct rtp_packet_t
{
	rtp_header_t rtp;
	uint32_t csrc[16];
	const void* extension; // extension(valid only if rtp.x = 1)
	uint16_t extlen; // extension length in bytes
	uint16_t reserved; // extension reserved
	const void* payload; // payload
	int payloadlen; // payload length in bytes
};

///@return 0-ok, other-error
int rtp_packet_deserialize(struct rtp_packet_t *pkt, const void* data, int bytes);

///@return <0-error, >0-rtp packet size, =0-impossible
int rtp_packet_serialize(const struct rtp_packet_t *pkt, void* data, int bytes);

#endif /* !_rtp_packet_h_ */
