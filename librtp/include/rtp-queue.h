#ifndef _rtp_queue_h_
#define _rtp_queue_h_

void* rtp_queue_create();
int rtp_queue_destroy(void* queue);

int rtp_queue_lock(void* queue, void** ptr, int size);
int rtp_queue_unlock(void* queue, void* ptr, int size);

int rtp_queue_read(void* queue, void **rtp, int *len, int *lostPacket);
int rtp_queue_free(void* queue, void *rtp);

#endif /* !_rtp_queue_h_ */
