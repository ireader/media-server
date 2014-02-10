#ifndef _rtp_queue_h_
#define _rtp_queue_h_

RTP_API void* rtp_queue_create();
RTP_API int rtp_queue_destroy(void* queue);

RTP_API int rtp_queue_lock(void* queue, void** ptr, int size);
RTP_API int rtp_queue_unlock(void* queue, void* ptr, int size);

RTP_API int rtp_queue_read(void* queue);
RTP_API int rtp_queue_unread(void* queue);

#endif /* !_rtp_queue_h_ */
