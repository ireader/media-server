#ifndef _rtp_queue_h_
#define _rtp_queue_h_

#include "rtp-packet.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct rtp_queue_t rtp_queue_t;

rtp_queue_t* rtp_queue_create(int threshold, int frequency, void (*freepkt)(void*, struct rtp_packet_t*), void* param);
int rtp_queue_destroy(rtp_queue_t* queue);

int rtp_queue_write(rtp_queue_t* queue, struct rtp_packet_t* pkt);
struct rtp_packet_t* rtp_queue_read(rtp_queue_t* queue);

#if defined(__cplusplus)
}
#endif
#endif /* !_rtp_queue_h_ */
