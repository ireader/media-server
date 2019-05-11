#ifndef _avpacket_queue_h_
#define _avpacket_queue_h_

#include <stdint.h>

struct avpacket_t
{
	int stream; 
	int64_t pts;
	int64_t dts;

	uint8_t buffer[512*1024];
	int bytes;
};

struct avpacket_queue_t;

struct avpacket_queue_t* avpacket_queue_create(int size);
void avpacket_queue_destroy(struct avpacket_queue_t* q);

void avpacket_queue_clear(struct avpacket_queue_t* q);
int avpacket_queue_count(struct avpacket_queue_t* q);
int avpacket_queue_pop(struct avpacket_queue_t* q);

struct avpacket_t* avpacket_queue_front(struct avpacket_queue_t* q);
int avpacket_queue_push(struct avpacket_queue_t* q, const struct avpacket_t* pkt);

struct avpacket_t* avpacket_queue_front_wait(struct avpacket_queue_t* q, int ms);
int avpacket_queue_push_wait(struct avpacket_queue_t* q, const struct avpacket_t* pkt, int ms);

#endif /* !_avpacket_queue_h_*/
