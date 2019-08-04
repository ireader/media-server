#include "avpacket-queue.h"
#include "sys/sync.hpp"
#include <queue>

struct avpacket_queue_t
{
	std::queue<avpacket_t*>::size_type maxsize;
	std::queue<avpacket_t*> q;
	ThreadLocker locker;
	ThreadEvent event;
};

struct avpacket_queue_t* avpacket_queue_create(int size)
{
	struct avpacket_queue_t* q = new struct avpacket_queue_t;
	q->maxsize = size;
	return q;
}

void avpacket_queue_destroy(struct avpacket_queue_t* q)
{
	avpacket_queue_clear(q);
	delete q;
}

void avpacket_queue_clear(struct avpacket_queue_t* q)
{
	AutoThreadLocker locker(q->locker);
	while (!q->q.empty())
	{
		struct avpacket_t* pkt = q->q.front();
		avpacket_release(pkt);
		q->q.pop();
	}
}

int avpacket_queue_count(struct avpacket_queue_t* q)
{
	AutoThreadLocker locker(q->locker);
	return q->q.size();
}

int avpacket_queue_pop(struct avpacket_queue_t* q)
{
	struct avpacket_t* pkt;
	{
		AutoThreadLocker locker(q->locker);
		if (q->q.empty())
			return -1;

		pkt = q->q.front();
		q->q.pop();
		q->event.Signal();
	}

	avpacket_release(pkt);
	return 0;
}

struct avpacket_t* avpacket_queue_front(struct avpacket_queue_t* q)
{
	struct avpacket_t* pkt;
	AutoThreadLocker locker(q->locker);
	if (q->q.empty())
		return NULL;

	pkt = q->q.front();
	avpacket_addref(pkt);
	return pkt;
}

int avpacket_queue_push(struct avpacket_queue_t* q, struct avpacket_t* pkt)
{
	AutoThreadLocker locker(q->locker);
	if (q->q.size() >= q->maxsize)
		return -1;

	avpacket_addref(pkt);
	q->q.push(pkt);
	q->event.Signal();
	return 0;
}

struct avpacket_t* avpacket_queue_front_wait(struct avpacket_queue_t* q, int ms)
{
	q->locker.Lock();
	if (q->q.empty())
	{
		q->locker.Unlock();
		if (0 != q->event.TimeWait(ms))
			return NULL;
		q->locker.Lock();
	}
	
	if (q->q.empty())
	{
		q->locker.Unlock();
		return NULL;
	}

	struct avpacket_t* pkt = q->q.front();
	avpacket_addref(pkt);
	q->locker.Unlock();
	return pkt;
}

int avpacket_queue_push_wait(struct avpacket_queue_t* q, struct avpacket_t* pkt, int ms)
{
	q->locker.Lock();
	if (q->q.size() >= q->maxsize)
	{
		q->locker.Unlock();
		if (0 != q->event.TimeWait(ms))
			return -1;
		q->locker.Lock();
	}

	if (q->q.size() >= q->maxsize)
	{
		q->locker.Unlock();
		return -1;
	}

	avpacket_addref(pkt);
	q->q.push(pkt);
	q->locker.Unlock();
	return 0;
}
