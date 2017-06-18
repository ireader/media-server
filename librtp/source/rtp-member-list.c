#include "rtp-member-list.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#define N_SOURCE 2 // unicast(1S + 1R)

struct rtp_member_list
{
	struct rtp_member *members[N_SOURCE];
	struct rtp_member **ptr;
	int count;
	int capacity;
};

void* rtp_member_list_create()
{
	return (struct rtp_member_list *)calloc(1, sizeof(struct rtp_member_list));
}

void rtp_member_list_destroy(void* members)
{
	int i;
	struct rtp_member_list *p;
	p = (struct rtp_member_list *)members;

	for(i = 0; i < p->count; i++)
	{
		rtp_member_release(i >= N_SOURCE ? p->ptr[i-N_SOURCE] : p->members[i]);
	}

	if(p->ptr)
	{
		assert(p->capacity > 0);
		free(p->ptr);
	}

	free(p);
}

int rtp_member_list_count(void* members)
{
	struct rtp_member_list *p;
	p = (struct rtp_member_list *)members;
	return p->count;
}

struct rtp_member* rtp_member_list_get(void* members, int index)
{
	struct rtp_member_list *p;
	p = (struct rtp_member_list *)members;
	if(index >= p->count)
		return NULL;

	return index >= N_SOURCE ? p->ptr[index-N_SOURCE] : p->members[index];
}

struct rtp_member* rtp_member_list_find(void* members, uint32_t ssrc)
{
	int i;
	struct rtp_member *s;
	struct rtp_member_list *p;
	p = (struct rtp_member_list *)members;

	for(i = 0; i < p->count; i++)
	{
		s = i < N_SOURCE ? p->members[i] : p->ptr[i-N_SOURCE];
		if(s->ssrc == ssrc)
			return s;
	}
	return NULL;
}

int rtp_member_list_add(void* members, struct rtp_member* s)
{
	struct rtp_member_list *p;
	p = (struct rtp_member_list *)members;

	if(p->count >= N_SOURCE)
	{
		if(p->count - N_SOURCE >= p->capacity)
		{
			void* ptr;
			ptr = (struct rtp_member **)realloc(p->ptr, (p->capacity+8)*sizeof(struct rtp_member*));
			if(!ptr)
				return ENOMEM;
			p->ptr = ptr;
			p->capacity += 8;
		}
		p->ptr[p->count++ - N_SOURCE] = s;
	}
	else
	{
		p->members[p->count++] = s;
	}

	rtp_member_addref(s);
	return 0;
}

int rtp_member_list_delete(void* members, uint32_t ssrc)
{
	int i;
	struct rtp_member *s;
	struct rtp_member_list *p;
	p = (struct rtp_member_list *)members;

	for(i = 0; i < p->count; i++)
	{
		s = i < N_SOURCE ? p->members[i] : p->ptr[i-N_SOURCE];
		if(s->ssrc != ssrc)
			continue;

		if(i < N_SOURCE)
		{
			if(i+1 < N_SOURCE)
			{
				memmove(p->members + i, p->members+i+1, N_SOURCE-i-1);
			}

			if(p->count > N_SOURCE)
			{
				p->members[N_SOURCE-1] = p->ptr[0];
				memmove(p->ptr, p->ptr + 1, p->count-N_SOURCE-1);
			}
		}
		else
		{
			if(i + 1 < p->count)
			{
				memmove(p->ptr + i - N_SOURCE, p->ptr + i + 1 - N_SOURCE, p->count-i-1);
			}
		}

		--p->count;

		rtp_member_release(s);
		return 0;
	}

	return -1; // NOT_FOUND
}
