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

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

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
	struct rtp_item_t bad_items[RTP_SEQUENTIAL+1];

	int threshold;
	int frequency;
	void (*free)(void*, struct rtp_packet_t*);
	void* param;

	struct rtp_queue_stats_t stats;
};

static void rtp_queue_reset(struct rtp_queue_t* q);
static int rtp_queue_find(struct rtp_queue_t* q, uint16_t seq);
static int rtp_queue_insert(struct rtp_queue_t* q, int position, struct rtp_packet_t* pkt);

struct rtp_queue_t* rtp_queue_create(int threshold, int frequency, void(*freepkt)(void*, struct rtp_packet_t*), void* param)
{
	struct rtp_queue_t* q;
	q = (struct rtp_queue_t*)calloc(1, sizeof(*q));
	if(!q)
		return NULL;

	rtp_queue_reset(q);
	q->probation = 1;
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

static inline void rtp_queue_reset_bad_items(struct rtp_queue_t* q)
{
	int i;
	struct rtp_packet_t* pkt;

	for (i = 0; i < q->bad_count; i++)
	{
		pkt = q->bad_items[i].pkt;
		q->free(q->param, pkt);
	}

	q->bad_seq = 0;
	q->bad_count = 0;
}

static void rtp_queue_reset(struct rtp_queue_t* q)
{
	int i;
	struct rtp_packet_t* pkt;

	rtp_queue_reset_bad_items(q);

	for (i = 0; i < q->size; i++)
	{
		pkt = q->items[(q->pos + i) % q->capacity].pkt;
		q->free(q->param, pkt);
	}

	q->pos = 0;
	q->size = 0;
	q->probation = RTP_SEQUENTIAL;
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

	return l; // insert position
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
		p = realloc(q->items, capacity * sizeof(struct rtp_item_t));
		if (NULL == p)
			return -ENOMEM;

		q->items = (struct rtp_item_t*)p;
		if (q->pos + q->size > q->capacity)
		{
			// move to tail
			assert(q->pos < q->capacity);
			memmove(&q->items[q->pos + capacity - q->capacity], &q->items[q->pos], (q->capacity - q->pos) * sizeof(struct rtp_item_t));
			q->pos += capacity - q->capacity;
            position += capacity - q->capacity;
		}

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

/*
            first               last
              ^                  ^
---too late---|------------------|----max drop---|-----another sequential---
--------------|------queue-------|-------------------------------------------->
*/
int rtp_queue_write(struct rtp_queue_t* q, struct rtp_packet_t* pkt)
{
	int i, idx;
	uint16_t delta;

	q->stats.total++;
	if (q->probation)
	{
		if (q->size > 0 && (uint16_t)pkt->rtp.seq == q->last_seq + 1)
		{
			if (0 == --q->probation)
				q->first_seq = (uint16_t)q->items[q->pos].pkt->rtp.seq;
		}
		else if (q->size == 0 && q->probation == 1)
		{
			// init
			q->first_seq = (uint16_t)pkt->rtp.seq;
			--q->probation;
		}
		else
		{
			rtp_queue_reset(q);
		}

		q->last_seq = (uint16_t)pkt->rtp.seq;
		return rtp_queue_insert(q, q->pos + q->size, pkt);
	}
	else
	{
		delta = (uint16_t)(pkt->rtp.seq - q->last_seq);
		if (delta > 0 && delta < RTP_DROPOUT)
		{
			if (pkt->rtp.seq < q->last_seq)
				q->cycles += RTP_SEQMOD;

			rtp_queue_reset_bad_items(q);
			q->last_seq = (uint16_t)pkt->rtp.seq;
			return rtp_queue_insert(q, q->pos + q->size, pkt);
		}
		else if ( (int16_t)delta <= 0 && (int16_t)delta >= (int16_t)(q->first_seq - q->last_seq) )
		{
			// pkt->rtp.seq - q->first_seq < q->last_seq - q->first_seq

			// duplicate or reordered packet
			idx = rtp_queue_find(q, (uint16_t)pkt->rtp.seq);
			if (-1 == idx)
			{
				++q->stats.duplicate;
				return -1;
			}
			
			++q->stats.reorder;
			rtp_queue_reset_bad_items(q);
			return rtp_queue_insert(q, idx, pkt);
		}
		else if ((uint16_t)(q->first_seq - pkt->rtp.seq) < RTP_MISORDER)
		{
			// too late: pkt->req.seq < q->first_seq
			++q->stats.late;
			return -1;
		}
		else
		{
			if (q->bad_count > 0 && q->bad_seq == pkt->rtp.seq)
			{
				if (q->bad_count >= RTP_SEQUENTIAL)
				{
					// Two sequential packets -- assume that the other side
					// restarted without telling us so just re-sync
					// (i.e., pretend this was the first packet).
					
					//rtp_queue_reset(q);

					// copy saved items
					for (i = 0; i < q->bad_count; i++)
						rtp_queue_insert(q, q->pos + q->size, q->bad_items[i].pkt);

					q->bad_count = 0;
					q->last_seq = (uint16_t)pkt->rtp.seq;
					return rtp_queue_insert(q, q->pos + q->size, pkt);
				}
			}
			else
			{
				q->stats.bad++;
				rtp_queue_reset_bad_items(q);
			}

			q->bad_seq = (pkt->rtp.seq + 1) % (RTP_SEQMOD-1);
			q->bad_items[q->bad_count++].pkt = pkt;
			return 1;
		}
	}

	// for safety
	assert(0);
	return -1;
}

struct rtp_packet_t* rtp_queue_read(struct rtp_queue_t* q)
{
	uint32_t threshold;
	struct rtp_packet_t* pkt;
	if (q->size < 1 || q->probation)
		return NULL;

	assert(q->pos < q->capacity);
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
		threshold = (q->items[(q->pos + q->size - 1) % q->capacity].pkt->rtp.timestamp - pkt->rtp.timestamp);
		threshold = (int32_t)threshold < 0 ? (uint32_t)(-(int32_t)threshold) : threshold; // fix h.264 b-frames pts order
		threshold = (uint32_t)(((uint64_t)threshold) * 1000 / (uint64_t)q->frequency);
		if (threshold < (uint32_t)q->threshold && q->size + 5 < MIN(RTP_DROPOUT, MAX_PACKET) )
			return NULL;

		q->stats.lost += pkt->rtp.seq - q->first_seq;
		q->first_seq = (uint16_t)(pkt->rtp.seq + 1);
		q->size--;
		q->pos = (q->pos + 1) % q->capacity;
		return pkt;
	}
}

void rtp_queue_stats(struct rtp_queue_t* q, struct rtp_queue_stats_t* stats)
{
	memcpy(stats, &q->stats, sizeof(*stats));
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

static void rtp_queue_test2(void)
{
    int i;
    uint16_t seq;
    rtp_queue_t* q;
    struct rtp_packet_t* pkt;

    static uint16_t s_seq[1000];

    q = rtp_queue_create(100, 90000, rtp_packet_free, NULL);

    for(i = 0; i < sizeof(s_seq)/sizeof(s_seq[0]); i++)
        s_seq[i] = (uint16_t)(45000 + i);
    
    // 45460, 45461, 45462, 45464, 45465, 45466, ...,
    // 45490, 45491, 45492, 45503, 45504, 45505, 45463,
    // 45506, 45507, 45493, 45494, 45495, 45496, 45497,
    // 45498, 45499, 45500, 45501, 45502, 45508, 45509, ...
    memmove(s_seq + 463, s_seq + 464, sizeof(s_seq[0]) * (509 - 464)); // lost 45463
    s_seq[492] = 45503;
    s_seq[493] = 45504;
    s_seq[494] = 45505;
    s_seq[495] = 45463;
    s_seq[496] = 45506;
    s_seq[497] = 45507;
    s_seq[498] = 45493;
    s_seq[499] = 45494;
    s_seq[500] = 45495;
    s_seq[501] = 45496;
    s_seq[502] = 45497;
    s_seq[503] = 45498;
    s_seq[504] = 45499;
    s_seq[505] = 45500;
    s_seq[506] = 45501;
    s_seq[507] = 45502;
    s_seq[508] = 45508;
    
    seq = s_seq[0];
    for (i = 0; i < sizeof(s_seq) / sizeof(s_seq[0]); i++)
    {
        rtp_queue_packet(q, s_seq[i]);
        pkt = rtp_queue_read(q);
        if (pkt)
        {
            //printf("%u ", pkt->rtp.seq);
            assert(0 == pkt->rtp.seq - seq++);
            free(pkt);
        }
    }

	assert(q->stats.total == sizeof(s_seq) / sizeof(s_seq[0]) && q->stats.reorder == 11 && q->stats.lost == 0 && q->stats.bad == 0 && q->stats.duplicate == 0 && q->stats.late == 0);
    rtp_queue_destroy(q);
}

static void rtp_queue_test3(void)
{
	int i;
	uint16_t seq;
	rtp_queue_t* q;
	struct rtp_packet_t* pkt;

	static uint16_t s_seq[] = {
		1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
		11,12,13,14,15,16,17,18,19,20,
		21,22,
		31,32,33,34,35,36,37,38,39,40,
		41,42,43,44,45,
			  23,24,25,26,27,28,29,30,
					   46,47,48,49,50
	};
	q = rtp_queue_create(100, 90000, rtp_packet_free, NULL);

	seq = s_seq[0];
	for (i = 0; i < sizeof(s_seq) / sizeof(s_seq[0]); i++)
	{
		rtp_queue_packet(q, s_seq[i]);
		pkt = rtp_queue_read(q);
		if (pkt)
		{
			//printf("%u ", pkt->rtp.seq);
			assert(0 == pkt->rtp.seq - seq++);
			free(pkt);
		}
	}

	assert(q->stats.total == sizeof(s_seq) / sizeof(s_seq[0]) && q->stats.reorder == 8 && q->stats.lost == 0 && q->stats.bad == 0 && q->stats.duplicate == 0 && q->stats.late == 0);
	rtp_queue_destroy(q);
}

static void rtp_queue_test4(void)
{
	int i;
	uint16_t seq;
	rtp_queue_t* q;
	struct rtp_packet_t* pkt;

	// first packet
	static uint16_t s_seq[] = {
		1, 
						  17,18,19,20,
		21,22,
		   2, 3, 4, 5, 6, 7, 8, 9, 10,
		11,12,13,14,15,16,
			  23,24,25,26,27,28,29,30,
		31,32,33,34,35,36,37,38,39,40,
		41,42,43,44,45,46,47,48,49,50
	};
	q = rtp_queue_create(100, 90000, rtp_packet_free, NULL);

	seq = s_seq[0];
	for (i = 0; i < sizeof(s_seq) / sizeof(s_seq[0]); i++)
	{
		rtp_queue_packet(q, s_seq[i]);
		pkt = rtp_queue_read(q);
		if (pkt)
		{
			//printf("%u ", pkt->rtp.seq);
			assert(0 == pkt->rtp.seq - seq++);
			free(pkt);
		}
	}

	assert(q->stats.total == sizeof(s_seq) / sizeof(s_seq[0]) && q->stats.reorder == 15 && q->stats.lost == 0 && q->stats.bad == 0 && q->stats.duplicate == 0 && q->stats.late == 0);
	rtp_queue_destroy(q);
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

    rtp_queue_test2();
	rtp_queue_test3();
	rtp_queue_test4();
    
	q = rtp_queue_create(100, 90000, rtp_packet_free, NULL);

	for (i = 0; i < sizeof(s_seq) / sizeof(s_seq[0]); i++)
	{
		rtp_queue_packet(q, s_seq[i]);
		rtp_queue_dump(q);
		pkt = rtp_queue_read(q);
		if (pkt) free(pkt);
		rtp_queue_dump(q);
	}

	assert(q->stats.total == sizeof(s_seq)/sizeof(s_seq[0]) && q->stats.lost == 0 && q->stats.bad == 1 && q->stats.duplicate == 1 && q->stats.late == 20 && q->stats.reorder == 6);
	rtp_queue_destroy(q);
}
#endif
