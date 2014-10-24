#ifndef _rtp_avp_udp_receiver_
#define _rtp_avp_udp_receiver_

#include "sys/sock.h"

typedef void (*rtp_avp_udp_onrecv)(void* param, const void* data, size_t bytes);

void* rtp_avp_udp_receiver_create(void* param, socket_t rtp, socket_t rtcp, rtp_avp_udp_onrecv callback, void* param);
void rtp_avp_udp_receiver_destroy(void* receiver);

#endif /* !_rtp_avp_udp_receiver_ */
