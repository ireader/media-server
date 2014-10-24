#ifndef _rtp_socket_h_
#define _rtp_socket_h_

#include "sys/sock.h"

void rtp_socket_set_port_range(unsigned short base, unsigned short num);
void rtp_socket_get_port_range(unsigned short *base, unsigned short *num);

void rtp_socket_set_multicast_range(const char* multicast, unsigned int num);
void rtp_socket_get_multicast_range(char multicast[32], unsigned int *num);

int rtp_socket_create(const char* ip, socket_t *rtp, socket_t *rtcp, unsigned short *port);

#endif /* !_rtp_socket_h_ */
