#ifndef _rtp_avp_unicast_sender_h_
#define _rtp_avp_unicast_sender_h_

#ifdef __cplusplus
extern "C" {
#endif

#include "sys/sock.h"

void* rtp_avp_unicast_sender_create(const char* ip, u_short port[2], socket_t socket[2]);
void rtp_avp_unicast_sender_destroy(void* sender);

/// send RTP package
/// @param[in] data RTP package
/// @param[in] bytes RTP package length in byte
int rtp_avp_unicast_sender_send(void* sender, const void* data, size_t bytes);

void rtp_avp_unicast_sender_set_name(void *sender, const char* name);
void rtp_avp_unicast_sender_set_cname(void *sender, const char* cname);
void rtp_avp_unicast_sender_set_email(void *sender, const char* email);
void rtp_avp_unicast_sender_set_phone(void *sender, const char* phone);
void rtp_avp_unicast_sender_set_loc(void *sender, const char* loc);

#ifdef __cplusplus
}
#endif
#endif /* !_rtp_avp_unicast_sender_h_ */
