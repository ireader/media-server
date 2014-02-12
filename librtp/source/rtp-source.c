#include "rtp-source.h"
#include "cstringext.h"
#include "sys/sync.h"
#include <stdio.h>

struct rtp_source* rtp_source_create(unsigned int ssrc)
{
	struct rtp_source* p;
	p = (struct rtp_source*)malloc(sizeof(struct rtp_source));
	if(!p)
		return NULL;

	memset(p, 0, sizeof(struct rtp_source));
	p->ssrc = ssrc;
	return p;
}

void rtp_source_release(struct rtp_source *source)
{
	if(0 == InterlockedDecrement(&source->ref))
	{
		if(source->cname)
			free(source->cname);
		if(source->name)
			free(source->name);
		if(source->email)
			free(source->email);
		if(source->phone)
			free(source->phone);
		if(source->loc)
			free(source->loc);
		if(source->tool)
			free(source->tool);
		if(source->note)
			free(source->note);

		free(source);
	}
}

static void rtp_source_setvalue(char **p, const char* data, int bytes)
{
	if(*p)
	{
		int n = strlen(*p);
		if(n == bytes && 0 == strncmp(*p, data, bytes))
			return;

		free(*p);
	}
	*p = strndup(data, bytes);
}

void rtp_source_setcname(struct rtp_source *source, const char* data, int bytes)
{
	rtp_source_setvalue(&source->cname, data, bytes);
}

void rtp_source_setname(struct rtp_source *source, const char* data, int bytes)
{
	rtp_source_setvalue(&source->name, data, bytes);
}

void rtp_source_setemail(struct rtp_source *source, const char* data, int bytes)
{
	rtp_source_setvalue(&source->email, data, bytes);
}

void rtp_source_setphone(struct rtp_source *source, const char* data, int bytes)
{
	rtp_source_setvalue(&source->phone, data, bytes);
}

void rtp_source_setloc(struct rtp_source *source, const char* data, int bytes)
{
	rtp_source_setvalue(&source->loc, data, bytes);
}

void rtp_source_settool(struct rtp_source *source, const char* data, int bytes)
{
	rtp_source_setvalue(&source->tool, data, bytes);
}

void rtp_source_setnote(struct rtp_source *source, const char* data, int bytes)
{
	rtp_source_setvalue(&source->note, data, bytes);
}
