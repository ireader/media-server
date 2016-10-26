#include "cstringext.h"
#include "sys/locker.h"
#include "rtp-internal.h"
#include "rtp-util.h"
#include "rtp-queue.h"
#include <stdlib.h>
#include <memory.h>
#include <assert.h>

#define MAX_SIZE 128*1024*1024

#define INVALID_SEQ 0xFFFFFFFF
#define SEQLT(l, r) ( (unsigned short)(r) - (unsigned short)(l) < 0x8000 )

struct rtp_frame
{
	struct rtp_frame *next;
	unsigned short seq;
//	unsigned int timestamp;
//	unsigned int ssrc;
	unsigned char *ptr;	
	int capacity;
	int len;
	time64_t clock;
};

struct rtp_queue
{
	struct rtp_frame* head;
	struct rtp_frame* tail;
	int size;
	unsigned short expected; // expected seq
	unsigned int threshold; // default 100ms

	locker_t locker;
};

void* rtp_queue_create()
{
	struct rtp_queue* q;
	q = (struct rtp_queue*)malloc(sizeof(struct rtp_queue));
	if(!q)
		return (void*)0;

	memset(q, 0, sizeof(struct rtp_queue));
	locker_create(&q->locker);
	q->expected = 0xFFFF;
	q->threshold = 100;
	return q;
}

int rtp_queue_destroy(void* queue)
{
	struct rtp_queue *q;
	struct rtp_frame *frame, *next;

	if(!queue) return -1;
	q = (struct rtp_queue *)queue;
	for(frame = q->head; frame; frame = next)
	{
		next = frame->next;
		free(frame);
	}

	locker_destroy(&q->locker);
	free(q);
	return 0;
}

static int rtp_queue_push(struct rtp_queue* q, struct rtp_frame* frame)
{
	/// 1. frame > tail: append or drop(max drop)
	/// 2. frame < expect: late
	/// 3. expect < frame < tail: insert to list
	/// 4. frame in list: duplicate
	if(!q->head)
	{
		assert(!q->tail);
		q->head = q->tail = frame;
	}
	else
	{
		assert(!q->tail->next);
		assert(SEQLT(q->head->seq, q->tail->seq) || q->head->seq == q->tail->seq);
		if(SEQLT(q->tail->seq, frame->seq))
		{
			if(frame->seq - q->tail->seq > RTP_MISORDER)
			{
				// something wrong, drop frame
				free(frame);
				return -1;
			}
			else
			{
				// append
				q->tail->next = frame;
				q->tail = frame;
			}
		}
		else if(SEQLT(frame->seq, q->head->seq))
		{
			// insert before head
			if(q->head->seq - frame->seq > RTP_MISORDER)
			{
				// something wrong, drop frame
				free(frame);
				return -1;
			}
			else if(SEQLT(q->expected, q->head->seq) && q->head->seq - q->expected < RTP_MISORDER)
			{
				if(SEQLT(frame->seq, q->expected))
				{
					// too late, ignore
					free(frame);
					return -1;
				}
			}

			frame->next = q->head;
			q->head = frame;
		}
		else
		{
			// insert
			struct rtp_frame **p;
			for(p = &q->head; *p ; p = &(*p)->next)
			{
				if(SEQLT(frame->seq, (*p)->seq))
					break; // found

				if(frame->seq == (*p)->seq)
				{
					// duplicate packet, ignore
					free(frame);
					return 1;
				}
			}

			// insert
			frame->next = *p;
			*p = frame;
		}
	}

	return 0;
}

static struct rtp_frame* rtp_queue_pop(struct rtp_queue* q)
{
	struct rtp_frame* frame = NULL;
	if(q->head && (q->head->seq == q->expected || time64_now() - q->head->clock > q->threshold))
	{
		frame = q->head;

		q->head = frame->next;
		if(q->tail == frame)
		{
			assert(!q->head);
			assert(!q->tail->next);
			q->tail = NULL;
		}
	}
	return frame;
}

int rtp_queue_lock(void* queue, void** ptr, int size)
{
	struct rtp_queue *q;
	struct rtp_frame* frame;
	q = (struct rtp_queue *)queue;

	if(size < 0 || size > MAX_SIZE)
		return -1; // invalid size

	frame = (struct rtp_frame*)malloc(sizeof(struct rtp_frame) + size);
	if(!frame)
		return -1; // alloc memory error

	memset(frame, 0, sizeof(*frame));
	frame->capacity = size;
	frame->ptr = (unsigned char*)(frame+1);
	frame->clock = time64_now();

	*ptr = frame->ptr;
	return 0;
}

//#if defined(_DEBUG)
//static void rtp_queue_dump(struct rtp_queue *q)
//{
//	char msg[1024] = {0};
//	struct rtp_frame *p;
//
//	for(p = q->head; p; p = p->next)
//	{
//		char seq[8];
//		sprintf(seq, "-%u", p->seq);
//		strcat(msg, seq);
//	}
//
//	strcat(msg, "\r\n");
//	OutputDebugStringA(msg);
//}
//#endif

int rtp_queue_unlock(void* queue, void* ptr, int size)
{
	unsigned int v;
	struct rtp_queue *q;
	struct rtp_frame *frame;
//	time64_t tnow =  time64_now();

	q = (struct rtp_queue *)queue;
	frame = (struct rtp_frame *)ptr - 1;
	assert(frame->ptr == (unsigned char *)ptr);

	v = nbo_r32((unsigned char *)ptr);
	frame->seq = RTP_SEQ(v);
//	frame->timestamp = ntohl(((const unsigned int *)ptr)[1]);
//	frame->ssrc = ntohl(((const unsigned int *)ptr)[2]);
	frame->clock = time64_now();
	frame->len = size;
	assert(!frame->next);

	locker_lock(&q->locker);
	rtp_queue_push(q, frame);
//	rtp_queue_dump(q);
	locker_unlock(&q->locker);
	return 0;
}

int rtp_queue_read(void* queue, void **rtp, int *len, int *lostPacket)
{
	struct rtp_queue *q;
	struct rtp_frame *frame;

	q = (struct rtp_queue *)queue;
	locker_lock(&q->locker);
	frame = rtp_queue_pop(q);
	locker_unlock(&q->locker);

	if(!frame)
		return -1;

#if 1
	if(frame->seq != q->expected)
	{
		char msg[64];
		sprintf(msg, "lost: %u-%u\n", q->expected, frame->seq);
//		OutputDebugString(msg);
	}
#endif

	*rtp = frame->ptr;
	*len = frame->len;
	*lostPacket = (frame->seq == q->expected) ? 0 : 1;

	q->expected = frame->seq + 1; // update expected
	return 0;
}

int rtp_queue_free(void* queue, void *rtp)
{
	struct rtp_queue *q;
	struct rtp_frame *frame;

	q = (struct rtp_queue *)queue;
	frame = (struct rtp_frame *)rtp - 1;
	assert(frame->ptr == (unsigned char *)rtp);

	free(frame);
	return 0;
}
