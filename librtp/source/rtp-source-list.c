#include "cstringext.h"
#include "sys/atomic.h"
#include "rtp-source-list.h"
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <assert.h>

#define N_SOURCE 2 // unicast(1S + 1R)

struct rtp_source_list
{
	struct rtp_source *sources[N_SOURCE];
	struct rtp_source **ptr;
	int count;
	int capacity;
};

void* rtp_source_list_create()
{
	struct rtp_source_list *p;
	p = (struct rtp_source_list *)malloc(sizeof(struct rtp_source_list));
	if(!p)
		return NULL;

	memset(p, 0, sizeof(struct rtp_source_list));
	return p;
}

void rtp_source_list_destroy(void* members)
{
	struct rtp_source_list *p;
	p = (struct rtp_source_list *)members;
	if(p->ptr)
	{
		assert(p->capacity > 0);
		free(p->ptr);
	}
	free(p);
}

int rtp_source_list_count(void* members)
{
	struct rtp_source_list *p;
	p = (struct rtp_source_list *)members;
	return p->count;
}

struct rtp_source* rtp_source_list_get(void* members, int index)
{
	struct rtp_source_list *p;
	p = (struct rtp_source_list *)members;
	if(index < 0 || index >= p->count)
		return NULL;

	return index >= N_SOURCE ? p->ptr[index-N_SOURCE] : p->sources[index];
}

struct rtp_source* rtp_source_list_find(void* members, unsigned int ssrc)
{
	int i;
	struct rtp_source *s;
	struct rtp_source_list *p;
	p = (struct rtp_source_list *)members;

	for(i = 0; i < p->count; i++)
	{
		s = i < N_SOURCE ? p->sources[i] : p->ptr[i-N_SOURCE];
		if(s->ssrc == ssrc)
			return s;
	}
	return NULL;
}

int rtp_source_list_add(void* members, struct rtp_source* s)
{
	struct rtp_source_list *p;
	p = (struct rtp_source_list *)members;

	//s = rtp_source_create(ssrc);
	//if(!s)
	//	return NULL;

	if(p->count >= N_SOURCE)
	{
		if(p->count - N_SOURCE >= p->capacity)
		{
			p->ptr = (struct rtp_source **)realloc(p->ptr, (p->capacity+8)*sizeof(struct rtp_source*));
			if(!p->ptr)
			{
//				rtp_source_release(s);
				return ENOMEM;
			}
			p->capacity += 8;
		}
		p->ptr[p->count++ - N_SOURCE] = s;
	}
	else
	{
		p->sources[p->count++] = s;
	}

	atomic_increment32(&s->ref);
	return 0;
}

int rtp_source_list_delete(void* members, unsigned int ssrc)
{
	int i;
	struct rtp_source *s;
	struct rtp_source_list *p;
	p = (struct rtp_source_list *)members;

	for(i = 0; i < p->count; i++)
	{
		s = i < N_SOURCE ? p->sources[i] : p->ptr[i-N_SOURCE];
		if(s->ssrc != ssrc)
			continue;

		rtp_source_release(s);

		if(i < N_SOURCE)
			memmove(p->sources + i, p->sources+i+1, N_SOURCE-i);
		else
			memmove(p->ptr + i - N_SOURCE, p->ptr + i - N_SOURCE, p->count-N_SOURCE-i);

		--p->count;
        
        // TODO:
        //atomic_decrement32(&s->ref);
		return 0;
	}

	return -1; // NOT_FOUND
}
