#include "rtp-queue.h"
#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>

#define N 10000 // count
#define Q 400	// timestamp queue length, Q > RTP_MISORDER

#define RTP_LOST 10		// 10%
#define RTP_MISORDER 3	// 0.3%
#define RTP_DUPLICATE 3 // 3%
#define RTP_SEEK 5		// 0.05%

static uint16_t s_input_seq[N];
static uint16_t s_output_seq[N];

struct rtp_queue_test_t
{
	rtp_queue_t* q;
	
	int input_lost;
	int output_lost;

	int count_seek;
	int count_lost;
	int count_misorder;
	int count_duplicate;

	int count_input;
	int count_output;
};

static void rtp_packet_free(void* param, struct rtp_packet_t* pkt)
{
	free(pkt); (void)param;
}

static struct rtp_packet_t* rtp_queue_packet_alloc(uint16_t seq, uint32_t timestamp)
{
	struct rtp_packet_t* pkt;
	pkt = (struct rtp_packet_t*)calloc(1, sizeof(*pkt));
	if (!pkt) return pkt;

	pkt->rtp.seq = seq;
	pkt->rtp.timestamp = timestamp;
	return pkt;
}

void rtp_queue_test2(void)
{
	std::vector<uint32_t> buckets;
	struct rtp_queue_test_t test;
	struct rtp_packet_t* pkt;
	memset(&test, 0, sizeof(test));

	test.q = rtp_queue_create(200, 90000, rtp_packet_free, NULL);
	
	//srand((unsigned int)time(NULL));
	srand(1);
	uint32_t clock = rand() * 90;
	printf("begin seq: %hu\n", (uint16_t)(clock / 10));
	for (int i = 0; i < N; i++)
	{
		int kick = rand();

		// [9995 ~ 9999]
		if (kick % 10000 > 10000 - RTP_SEEK)
		{
			clock += rand() * 90;
			printf("seek seq: %hu\n", (uint16_t)(clock / 10));

			test.count_seek++;
		}

		do
		{
			buckets.push_back(clock);
			clock += 10; // 100fps
		} while (buckets.size() < Q);

		uint32_t timestamp = buckets.front();
		uint16_t seq = (uint16_t)(timestamp / 10);
		buckets.erase(buckets.begin());

		// [0 ~ 9]
		if (kick % 100 < RTP_LOST)
		{
			printf("lost: %hu\n", seq);
			test.count_lost++;
			continue;
		}

		// [997 ~ 999]
		if (kick % 1000 > 1000 - RTP_MISORDER)
		{
			printf("misorder: %hu\n", seq);
			buckets.insert(buckets.begin() + (seq % 10), timestamp);
			test.count_misorder++;
			continue;
		}

		// [97 ~ 99]
		if (kick % 100 > 100 - RTP_DUPLICATE)
		{
			printf("duplicate: %hu\n", seq);
			buckets.insert(buckets.begin() + (seq % 10), timestamp);
			test.count_duplicate++;
		}

		pkt = rtp_queue_packet_alloc(seq, timestamp * 90);
		s_input_seq[test.count_input++] = pkt->rtp.seq;

		if (rtp_queue_write(test.q, pkt) < 1)
		{
			printf("discard: %hu\n", pkt->rtp.seq);
			free(pkt);
			continue;
		}

		pkt = rtp_queue_read(test.q);
		if (!pkt)
			continue;

		s_output_seq[test.count_output++] = pkt->rtp.seq;
		free(pkt);
	}

	rtp_queue_destroy(test.q);

	printf("result: \n");
	int j = 0;
	for (int i = 0; i < test.count_input; i++)
	{
		if (s_input_seq[i] == s_output_seq[j])
		{
			printf("%hu, ", s_input_seq[i]);
			j++;
		}
		else
		{
			// try find misorder
			int found = 0;
			for (int k = i + 1; k < test.count_input && k < i + 20; k++)
			{
				if (s_input_seq[k] == s_output_seq[j])
				{
					j++;
					printf("%hu, ", s_input_seq[i]);
					found = 1;
					break;
				}
			}

			if(0 == found)
				printf("[%hu], ", s_input_seq[i]);
		}
	}

	//assert(test.input_lost == test.output_lost);
}
