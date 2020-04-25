// RFC2326 A.1 RTP Data Header Validity Checks

#include "rtp-queue.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#define MAX_PACKET 3000

#define RTP_MISORDER 300
#define RTP_DROPOUT  1000
#define RTP_SEQUENTIAL 3
#define RTP_SEQMOD	 (1 << 16)

struct rtp_item_t
{
	struct rtp_packet_t* pkt;
//	uint64_t clock;
};

struct rtp_queue_t
{
	struct rtp_item_t* items;
	int capacity;
	int size;
	int pos; // ring buffer read position

	int probation;
	int cycles;
	uint16_t last_seq;
	uint16_t first_seq;

	int bad_count;
	uint16_t bad_seq;

	int threshold;
	int frequency;
	void (*free)(void*, struct rtp_packet_t*);
	void* param;
};

static int rtp_queue_reset(struct rtp_queue_t* q);
static int rtp_queue_find(struct rtp_queue_t* q, uint16_t seq);
static int rtp_queue_insert(struct rtp_queue_t* q, int position, struct rtp_packet_t* pkt);

struct rtp_queue_t* rtp_queue_create(int threshold, int frequency, void(*freepkt)(void*, struct rtp_packet_t*), void* param)
{
	struct rtp_queue_t* q;
	q = (struct rtp_queue_t*)calloc(1, sizeof(*q));
	if(!q)
		return NULL;

	rtp_queue_reset(q);
	q->threshold = threshold;
	q->frequency = frequency;
	q->free = freepkt;
	q->param = param;
	return q;
}

int rtp_queue_destroy(struct rtp_queue_t* q)
{
	rtp_queue_reset(q);

	if (q->items)
	{
		assert(q->capacity > 0);
		free(q->items);
		q->items = 0;
	}
	free(q);
	return 0;
}

static int rtp_queue_reset(struct rtp_queue_t* q)
{
	int i;
	struct rtp_packet_t* pkt;

	for (i = 0; i < q->size; i++)
	{
		pkt = q->items[q->pos + i].pkt;
		q->free(q->param, pkt);
	}

	q->pos = 0;
	q->size = 0;
	q->bad_seq = 0;
	q->bad_count = 0;
	q->probation = RTP_SEQUENTIAL;
	return 0;
}

static int rtp_queue_find(struct rtp_queue_t* q, uint16_t seq)
{
	uint16_t v;
	uint16_t vi;
	int l, r, i;

	l = q->pos;
	r = q->pos + q->size;
	v = q->last_seq - seq;
	while (l < r)
	{
		i = (l + r) / 2;
		vi = (uint16_t)q->last_seq - (uint16_t)q->items[i % q->capacity].pkt->rtp.seq;
		if (vi == v)
		{
			return -1; // duplicate
		}
		else if (vi < v)
		{
			r = i;
		}
		else
		{
			assert(vi > v);
			l = i + 1;
		}
	}

	return l % q->capacity; // insert position
}

static int rtp_queue_insert(struct rtp_queue_t* q, int position, struct rtp_packet_t* pkt)
{
	void* p;
	int i, capacity;

	assert(position >= q->pos && position <= q->pos + q->size);

	if (q->size >= q->capacity)
	{
		if (q->size + 1 > MAX_PACKET)
			return -E2BIG;

		capacity = q->capacity + 250;
		p = realloc(q->items, capacity);
		if (NULL == p)
			return -ENOMEM;

		q->items = (struct rtp_item_t*)p;
		for (i = q->capacity; i < q->pos + q->size; i++)
			memcpy(&q->items[i % capacity], &q->items[i % q->capacity], sizeof(struct rtp_item_t));
		q->capacity = capacity;
	}

	// move items
	for (i = q->pos + q->size; i > position; i--)
		memcpy(&q->items[i % q->capacity], &q->items[(i - 1) % q->capacity], sizeof(struct rtp_item_t));

	q->items[position % q->capacity].pkt = pkt;
//	q->items[position % q->capacity].clock = 0;
	q->size++;
	return 1;
}

int rtp_queue_write(struct rtp_queue_t* q, struct rtp_packet_t* pkt)
{
	int idx;
	uint16_t delta;

	if (q->probation)
	{
		if (q->size > 0 && (uint16_t)pkt->rtp.seq == q->last_seq + 1)
		{
			q->probation--;
			if (0 == q->probation)
				q->first_seq = (uint16_t)q->items[q->pos].pkt->rtp.seq;
		}
		else
		{
			rtp_queue_reset(q);
		}

		q->last_seq = (uint16_t)pkt->rtp.seq;
		rtp_queue_insert(q, q->pos + q->size, pkt);
		return 1;
	}
	else
	{
		delta = (uint16_t)pkt->rtp.seq - q->last_seq;
		if (delta < RTP_DROPOUT)
		{
			if (pkt->rtp.seq < q->last_seq)
				q->cycles += RTP_SEQMOD;

			q->bad_count = 0;
			q->last_seq = (uint16_t)pkt->rtp.seq;
			return rtp_queue_insert(q, q->pos + q->size, pkt);
		}
		else if (delta < (uint16_t)(q->first_seq - q->last_seq))
		{
			// too late: pkt->req.seq < q->first_seq
			return 0;
		}
		else if (delta <= RTP_SEQMOD - RTP_MISORDER)
		{
			if (q->bad_seq == pkt->rtp.seq)
			{
				if (++q->bad_count >= RTP_SEQUENTIAL + 1)
				{
					// Two sequential packets -- assume that the other side
					// restarted without telling us so just re-sync
					// (i.e., pretend this was the first packet).
					rtp_queue_reset(q);
					q->last_seq = (uint16_t)pkt->rtp.seq;
					return rtp_queue_insert(q, q->pos + q->size, pkt);
				}
			}
			else
			{
				q->bad_count = 0;
			}

			q->bad_seq = (pkt->rtp.seq + 1) % (RTP_SEQMOD-1);
			return 0;
		}
		else
		{
			// duplicate or reordered packet
			idx = rtp_queue_find(q, (uint16_t)pkt->rtp.seq);
			if (-1 == idx)
				return 0;
			q->bad_count = 0;
			return rtp_queue_insert(q, idx, pkt);
		}
	}
}

struct rtp_packet_t* rtp_queue_read(struct rtp_queue_t* q)
{
	uint32_t threshold;
	struct rtp_packet_t* pkt;
	if (q->size < 1 || q->probation)
		return NULL;

	pkt = q->items[q->pos].pkt;
	if (q->first_seq == pkt->rtp.seq)
	{
		q->first_seq++;
		q->size--;
		q->pos = (q->pos + 1) % q->capacity;
		return pkt;
	}
	else
	{
		threshold = (q->items[(q->pos + q->size - 1) % q->capacity].pkt->rtp.timestamp - pkt->rtp.timestamp) / (q->frequency / 1000);
		if (threshold < (uint32_t)q->threshold)
			return NULL;

		q->first_seq = (uint16_t)pkt->rtp.seq + 1;
		q->size--;
		q->pos = (q->pos + 1) % q->capacity;
		return pkt;
	}
}

#if defined(_DEBUG) || defined(DEBUG)
#include <stdio.h>
static void rtp_queue_dump(struct rtp_queue_t* q)
{
	int i;
	printf("[%02d/%02d]: ", q->pos, q->size);
	for (i = 0; i < q->size; i++)
	{
		printf("%u\t", (unsigned int)q->items[(i + q->pos) % q->capacity].pkt->rtp.seq);
	}
	printf("\n");
}

static void rtp_packet_free(void* param, struct rtp_packet_t* pkt)
{
	free(pkt); (void)param;
}

static int rtp_queue_packet(rtp_queue_t* q, uint16_t seq)
{
	struct rtp_packet_t* pkt;
	pkt = (struct rtp_packet_t*)malloc(sizeof(*pkt));
	if (pkt)
	{
		memset(pkt, 0, sizeof(*pkt));
		pkt->rtp.seq = seq;
		if (0 == rtp_queue_write(q, pkt))
			free(pkt);
	}
	return 0;
}

void rtp_queue_test(void)
{
	int i;
	rtp_queue_t* q;
	struct rtp_packet_t* pkt;

	static uint16_t s_seq[] = {
		836, 837, 859, 860, 822, 823, 824, 825,
		826, 822, 830, 827, 831, 828, 829, 830,
		832, 833, 834, 6000, 840, 841, 842, 843,
		835, 836, 837, 838, 838, 844, 859, 811,
	};

	q = rtp_queue_create(100, 90000, rtp_packet_free, NULL);

	for (i = 0; i < sizeof(s_seq) / sizeof(s_seq[0]); i++)
	{
		rtp_queue_packet(q, s_seq[i]);
		rtp_queue_dump(q);
		pkt = rtp_queue_read(q);
		if (pkt) free(pkt);
		rtp_queue_dump(q);
	}

	rtp_queue_destroy(q);
}
#endif
