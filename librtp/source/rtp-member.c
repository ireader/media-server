#include "rtp-member.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct rtp_member* rtp_member_create(uint32_t ssrc)
{
	struct rtp_member* p;
	p = (struct rtp_member*)calloc(1, sizeof(struct rtp_member));
	if(!p)
		return NULL;

	p->ref = 1;
	p->ssrc = ssrc;
	p->jitter = 0.0;
	p->rtp_probation = RTP_PROBATION;
	p->rtcp_sr.ssrc = ssrc;
	p->rtcp_rb.ssrc = ssrc;
	return p;
}

void rtp_member_addref(struct rtp_member *member)
{
	++member->ref;
}

void rtp_member_release(struct rtp_member *member)
{
	if(0 == --member->ref)
	{
		size_t i;
		for(i = 0; i < sizeof(member->sdes)/sizeof(member->sdes[0]); i++)
		{
			if(member->sdes[i].data)
			{
				assert(member->sdes[i].pt == i);
				assert(member->sdes[i].len > 0);
				free(member->sdes[i].data);
			}
		}

		free(member);
	}
}

int rtp_member_setvalue(struct rtp_member *member, int item, const uint8_t* data, int bytes)
{
	rtcp_sdes_item_t *sdes;
	assert(RTCP_SDES_CNAME <= item && item <= RTCP_SDES_PRIVATE);
	if(item >= sizeof(member->sdes)/sizeof(member->sdes[0]) || bytes > 255)
		return -1;

	sdes = &member->sdes[item];

	if(bytes > sdes->len)
	{
		void* p = realloc(sdes->data, bytes);
		if(!p)
			return -1; // no memory
		sdes->data = p;
	}

	assert(bytes <= 255);
	if (bytes > 0)
		memcpy(sdes->data, data, bytes);
	sdes->pt = (uint8_t)item;
	sdes->len = (uint8_t)bytes;
	return 0;
}
