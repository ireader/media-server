#include "cstringext.h"
#include "rtp-member.h"
#include "sys/atomic.h"
#include <stdio.h>

struct rtp_member* rtp_member_create(uint32_t ssrc)
{
	struct rtp_member* p;
	p = (struct rtp_member*)malloc(sizeof(struct rtp_member));
	if(!p)
		return NULL;

	memset(p, 0, sizeof(struct rtp_member));
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
	atomic_increment32(&member->ref);
}

void rtp_member_release(struct rtp_member *member)
{
	if(0 == atomic_decrement32(&member->ref))
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

int rtp_member_setvalue(struct rtp_member *member, int item, const unsigned char* data, size_t bytes)
{
	rtcp_sdes_item_t *sdes;
	assert(RTCP_SDES_CNAME <= item && item <= RTCP_SDES_PRIVATE);
	if((size_t)item >= sizeof(member->sdes)/sizeof(member->sdes[0]) || bytes > 255)
		return -1;

	sdes = &member->sdes[item];

	if(bytes > sdes->len)
	{
		void* p = realloc(sdes->data, bytes);
		if(!p)
			return -1; // no memory
		sdes->data = p;
	}

	if(bytes > 0)
		memcpy(sdes->data, data, bytes);
	sdes->pt = (unsigned char)item;
	sdes->len = (unsigned char)bytes;
	return 0;
}
