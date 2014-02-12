#include "rtp-avp-udp.h"
#include "cstringext.h"
#include "sys/sock.h" // ntohl
//#include "rtp-queue.h"
#include "rtp.h"
#include <stdlib.h>
#include <memory.h>
#include <assert.h>

#define MAX_SIZE 128*1024*1024

struct rtp_frame
{
//	struct rtp_frame *prev;
	struct rtp_frame *next;
	unsigned short seq;
	unsigned int timestamp;
	unsigned int ssrc;
	unsigned char *ptr;	
	int capacity;
	int len;
};

struct rtp_queue
{
	struct rtp_frame* head;
	int size;
};

void* rtp_queue_create()
{
	struct rtp_queue* q;
	q = (struct rtp_queue*)malloc(sizeof(struct rtp_queue));
	if(!q)
		return (void*)0;

	memset(q, 0, sizeof(struct rtp_queue));
	return q;
}

int rtp_queue_destroy(void* queue)
{
	struct rtp_queue *q;
	struct rtp_frame *frame, *next;

	assert(queue);
	q = (struct rtp_queue *)queue;
	for(frame = q->head; frame; frame = next)
	{
		next = frame->next;
		free(frame);
	}

	free(q);
	return 0;
}

int rtp_queue_lock(void* queue, void** ptr, int size)
{
	struct rtp_frame* frame;
	if(size < 0 || size > MAX_SIZE)
		return -1; // invalid size

	frame = (struct rtp_frame*)malloc(sizeof(struct rtp_frame) + size);
	if(!frame)
		return -1; // alloc memory error

	memset(frame, 0, sizeof(frame));
	frame->capacity = size;
	frame->ptr = (unsigned char*)(frame+1);

	*ptr = frame->ptr;
	return 0;
}

int rtp_queue_unlock(void* queue, void* ptr, int size)
{
	unsigned int v;
	struct rtp_queue *q;
	struct rtp_frame *frame, **p;

	q = (struct rtp_queue *)queue;
	frame = (struct rtp_frame *)ptr - 1;
	assert(frame->ptr == (unsigned char *)ptr);

	v = ntohl(((const unsigned int *)ptr)[0]);
	frame->seq = RTP_SEQ(v);
	frame->timestamp = ntohl(((const unsigned int *)ptr)[1]);
	frame->ssrc = ntohl(((const unsigned int *)ptr)[2]);

	for(p = &q->head; *p ; p = &(*p)->next)
	{
		if(frame->seq < (*p)->seq)
			break; // found

		if(frame->seq == (*p)->seq)
		{
			// duplicate packet, ignore
			free(frame);
			return 0;
		}
	}

	// link it
	frame->next = *p;
	*p = frame;
	return 0;
}

int rtp_queue_read(void* queue)
{
	return 0;
}

int rtp_queue_unread(void* queue)
{
	return 0;
}
