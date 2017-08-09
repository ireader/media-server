#ifndef _rtp_socket_h_
#define _rtp_socket_h_

#if defined(__cplusplus)
extern "C" {
#endif

#include "sys/sock.h"

void rtp_socket_set_port_range(unsigned short base, unsigned short num);
void rtp_socket_get_port_range(unsigned short *base, unsigned short *num);

void rtp_socket_set_multicast_range(const char* multicast, unsigned int num);
void rtp_socket_get_multicast_range(char multicast[SOCKET_ADDRLEN], unsigned int *num);

int rtp_socket_create(const char* ip, socket_t rtp[2], unsigned short port[2]);

#if defined(__cplusplus)
}
#endif
#endif /* !_rtp_socket_h_ */
