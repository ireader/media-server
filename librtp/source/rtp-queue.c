#include "rtp-queue.h"

#define MAX_SIZE 128*1024*1024

struct rtp_frame
{
	struct rtp_frame *prev;
	struct rtp_frame *next;
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
		return NULL;

	memset(q, 0, sizeof(struct rtp_queue));
	return q;
}

int rtp_queue_destroy(void* queue)
{
	struct rtp_queue *q;
	struct rtp_frame *frame, *next;

	assert(q);
	q = (struct rtp_queue *)queue;
	for(frame = q->head; frame; frame = next)
	{
		next = frame->next;
		free(frame);
	}

	free(q);
	return 0;
}

int rtp_queue_lock(void** ptr, int size)
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

int rtp_queue_unlock(void* ptr, int size)
{
}

int rtp_queue_read()
{
	return 0;
}

int rtp_queue_unread()
{
	return 0;
}
