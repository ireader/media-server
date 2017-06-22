#ifndef _rtp_payload_helper_h_
#define _rtp_payload_helper_h_

#include "rtp-packet.h"
#include "rtp-payload.h"

struct rtp_payload_helper_t
{
	struct rtp_payload_t handler;
	void* cbparam;

	int lost; // wait for next frame
	int flags; // lost packet

	uint16_t seq; // rtp seq
	uint32_t timestamp;

	uint8_t* ptr;
	int size, capacity;
};

void* rtp_payload_helper_create(struct rtp_payload_t *handler, void* cbparam);
void rtp_payload_helper_destroy(void* helper);

int rtp_payload_check(struct rtp_payload_helper_t* helper, const struct rtp_packet_t* pkt);

int rtp_payload_write(struct rtp_payload_helper_t* helper, const struct rtp_packet_t* pkt);

int rtp_payload_onframe(struct rtp_payload_helper_t *helper);

#endif /* !_rtp_payload_helper_h_ */
