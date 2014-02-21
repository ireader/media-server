#include "sdp.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <assert.h>

#define inline static

enum { SDP_M_MEDIA_UNKOWN=0, SDP_M_MEDIA_AUDIO, SDP_M_MEDIA_VIDEO, SDP_M_MEDIA_TEXT, SDP_M_MEDIA_APPLICATION, SDP_M_MEDIA_MESSAGE };
enum { SDP_M_PROTO_UKNOWN=0, SDP_M_PROTO_UDP, SDP_M_PROTO_RTP_AVP, SDP_M_PROTO_RTP_SAVP };

#define N_EMAIL 1
#define N_PHONE 1
#define N_CONNECTION 1
#define N_BANDWIDTH 1
#define N_TIMING 1
#define N_REPEAT 1
#define N_TIMEZONE 1
#define N_REPEAT_OFFSET 1
#define N_ATTRIBUTE 5
#define N_MEDIA 2
#define N_MEDIA_FORMAT 5

struct sdp_connection
{
	char* network;
	char* addrtype;
	char* address;
};

struct sdp_origin
{
	char* username;
	char* session;
	char* session_version;
	struct sdp_connection c;
};

struct sdp_email
{
	char* email;
};

struct sdp_phone
{
	char* phone;
};

struct sdp_bandwidth
{
	char* bwtype;
	char* bandwidth;
};

struct bandwidths
{
	int count;
	struct sdp_bandwidth bandwidths[N_BANDWIDTH];
	struct sdp_bandwidth *ptr;
	int capacity;
};

struct sdp_repeat
{
	char* interval;
	char* duration;

	struct offset
	{
		int count;
		char *offsets[N_REPEAT_OFFSET];
		char **ptr;
		int capacity;
	} offsets;
};

struct sdp_timezone
{
	char* time;
	char* offset;
};

struct sdp_timing
{
	char* start;
	char* stop;

	struct repeat
	{
		int count;
		struct sdp_repeat repeats[N_REPEAT];
		struct sdp_repeat *ptr;
		int capacity;
	} r;

	struct timezone
	{
		int count;
		struct sdp_timezone timezones[N_TIMEZONE];
		struct sdp_timezone *ptr;
		int capacity;
	} z;
};

struct sdp_encryption
{
	char* method;
	char* key;
};

struct sdp_attribute
{
	char* name;
	char* value;
};

struct attributes
{
	int count;
	struct sdp_attribute attrs[N_ATTRIBUTE];
	struct sdp_attribute *ptr;
	int capacity;
};

struct sdp_media
{
	char* media;
	char* port;
	char* proto;
	struct format
	{
		int count;
		char *formats[N_MEDIA_FORMAT];
		char **ptr;
		int capacity;
	} fmt;

	char* i;
	struct connection
	{
		int count;
		struct sdp_connection connections[N_EMAIL];
		struct sdp_connection *ptr;
		int capacity;
	} c;

	struct attributes a;
	struct bandwidths b;
	struct sdp_encryption k;
};

struct sdp_context
{
	char *raw; // raw source string
	int offset; // parse offset

	int v;

	struct sdp_origin o;
	char* s;
	char* i;
	char* u;

	struct email
	{
		int count;
		struct sdp_email emails[N_EMAIL];
		struct sdp_email *ptr;
		int capacity;
	} e;

	struct phone
	{
		int count;
		struct sdp_phone phones[N_PHONE];
		struct sdp_phone *ptr;
		int capacity;
	} p;

	struct sdp_connection c;

	struct bandwidths b;

	struct timing
	{
		int count;
		struct sdp_timing times[N_TIMING];
		struct sdp_timing *ptr;
		int capacity;
	} t;

	struct sdp_encryption k;

	struct attributes a;

	struct media
	{
		int count;
		struct sdp_media medias[N_MEDIA];
		struct sdp_media *ptr;
		int capacity;
	} m;
};

inline void sdp_skip_space(struct sdp_context* sdp)
{
	char c = sdp->raw[sdp->offset];
	while(' ' == c || '\t' == c) 
		c = sdp->raw[++sdp->offset];
}

inline int sdp_token_word(struct sdp_context* sdp, const char* escape)
{
	int n = sdp->offset;
	char c = sdp->raw[sdp->offset];
	while(c && !strchr(escape, c))
	{
		c = sdp->raw[++sdp->offset];
	}

	return sdp->offset - n;
}

inline int sdp_token_crlf(struct sdp_context* sdp)
{
	if('\r' == sdp->raw[sdp->offset])
		++sdp->offset;
	
	if('\n' == sdp->raw[sdp->offset])
	{
		++sdp->offset;
		return 0;
	}

	// sdp end line
	if('\0' == sdp->raw[sdp->offset])
		return 0;

	return -1;
}

inline void trim_right(const char* s, int *len)
{
	//// left trim
	//while(*len > 0 && isspace(s[*pos]))
	//{
	//	--*len;
	//	++*pos;
	//}

	// right trim
	while(*len > 0 && ' '==(s[*len - 1]))
	{
		--*len;
	}
}

static struct sdp_timing* sdp_get_timing(struct sdp_context* sdp, int idx)
{
	if(idx >= sdp->t.count || idx < 0)
		return NULL;

	return idx >= N_TIMING ? &sdp->t.ptr[idx - N_TIMING] : &sdp->t.times[idx];
}

static struct sdp_media* sdp_get_media(struct sdp_context* sdp, int idx)
{
	if(idx >= sdp->m.count || idx < 0)
		return NULL;

	return idx >= N_MEDIA ? &sdp->m.ptr[idx - N_MEDIA] : &sdp->m.medias[idx];
}

// RFC4566 5.1
static int sdp_parse_version(struct sdp_context* sdp)
{
	char c;
	assert(sdp);

	sdp->v = 0;

	sdp_skip_space(sdp);

	c = sdp->raw[sdp->offset];
	while('0' <= c && c <= '9')
	{
		sdp->v = sdp->v * 10 + (c - '0');
		c = sdp->raw[++sdp->offset];
	}

	return sdp_token_crlf(sdp);
}

// RFC4566 5.2
// o=<username> <sess-id> <sess-version> <nettype> <addrtype> <unicast-address>
// <username> "-" if the originating host does not support the concept of user IDs.
// <sess-id> is a numeric string
// <sess-version> is a version number for this session description
// <nettype> IN
// <addrtype> IP4/IP6
static int sdp_parse_origin(struct sdp_context* sdp)
{
	int n[6];
	struct sdp_origin *o;

	o = &sdp->o;
	memset(o, 0, sizeof(struct sdp_origin));
	memset(n, 0, sizeof(n));

	sdp_skip_space(sdp);
	o->username = sdp->raw + sdp->offset;
	n[0] = sdp_token_word(sdp, " \t\r\n");

	sdp_skip_space(sdp);
	o->session = sdp->raw + sdp->offset;
	n[1] = sdp_token_word(sdp, " \t\r\n");

	sdp_skip_space(sdp);
	o->session_version = sdp->raw + sdp->offset;
	n[2] = sdp_token_word(sdp, " \t\r\n");

	sdp_skip_space(sdp);
	o->c.network = sdp->raw + sdp->offset;
	n[3] = sdp_token_word(sdp, " \t\r\n");

	sdp_skip_space(sdp);
	o->c.addrtype = sdp->raw + sdp->offset;
	n[4] = sdp_token_word(sdp, " \t\r\n");

	sdp_skip_space(sdp);
	o->c.address = sdp->raw + sdp->offset;
	n[5] = sdp_token_word(sdp, " \t\r\n");

	// check before assign '\0'
	if(0==sdp_token_crlf(sdp) && n[0]>0 && n[1]>0 && n[2]>0 && n[3]>0 && n[4]>0 && n[5]>0)
	{
		o->username[n[0]] = '\0';
		o->session[n[1]] = '\0';
		o->session_version[n[2]] = '\0';
		o->c.network[n[3]] = '\0';
		o->c.addrtype[n[4]] = '\0';
		o->c.address[n[5]] = '\0';
		return 0;
	}

	return -1;
}

// RFC4566 5.3
// s=<session name>
// There MUST be one and only one "s=" field per session description. can be empty
static int sdp_parse_session(struct sdp_context* sdp)
{
	int n = 0;

	sdp->s = sdp->raw + sdp->offset;

	n = sdp_token_word(sdp, "\r\n");
	if(0 != sdp_token_crlf(sdp))
		return -1;

	sdp->s[n] = '\0';
	return 0;
}

// RFC4566 5.4
// i=<session description>
// There MUST be at most one session-level "i=" field per session description,
// and at most one "i=" field per media.
// default UTF-8
static int sdp_parse_information(struct sdp_context* sdp)
{
	int n = 0;
	char **i;

	if(sdp->m.count > 0)
	{
		i = &sdp_get_media(sdp, sdp->m.count-1)->i;
	}
	else
	{
		i = &sdp->i;
	}

	*i = sdp->raw + sdp->offset;

	n = sdp_token_word(sdp, "\r\n");
	if(0 != sdp_token_crlf(sdp))
		return -1;

	(*i)[n] = '\0';
	return 0;
}

// RFC4566 5.5
// u=<uri>
// This field is OPTIONAL, but if it is present it MUST be
// specified before the first media field. 
static int sdp_parse_uri(struct sdp_context* sdp)
{
	int n = 0;

	// No more than one URI field is allowed per session description.
	assert(!sdp->u);

	sdp->u = sdp->raw + sdp->offset;

	n = sdp_token_word(sdp, "\r\n");
	if(0 != sdp_token_crlf(sdp))
		return -1;

	sdp->u[n] = '\0';
	return 0;
}

// RFC4566 5.6
// e=<email-address>
// p=<phone-number>
// OPTIONAL, If an email address or phone number is present, it MUST be specified
// before the first media field. More than one email or phone field can
// be given for a session description.
// p=+1 617 555-6011
// e=j.doe@example.com (Jane Doe)
// e=Jane Doe <j.doe@example.com>
static int sdp_parse_email(struct sdp_context* sdp)
{
	int n = 0;
	struct sdp_email *e;

	if(sdp->e.count >= N_EMAIL)
	{
		if(sdp->e.count - N_EMAIL >= sdp->e.capacity)
		{
			sdp->e.ptr = (struct sdp_email*)realloc(sdp->e.ptr, (sdp->e.capacity+8)*sizeof(struct sdp_email));
			if(!sdp->e.ptr)
				return ENOMEM;

			sdp->e.capacity += 8;
		}

		e = &sdp->e.ptr[sdp->e.count - N_EMAIL];
	}
	else
	{
		e = &sdp->e.emails[sdp->e.count];
	}

	memset(e, 0, sizeof(struct sdp_email));
	e->email = sdp->raw + sdp->offset;

	n = sdp_token_word(sdp, "\r\n");
	if(0 != sdp_token_crlf(sdp))
		return -1;

	e->email[n] = '\0';
	++sdp->e.count;
	return 0;
}

static int sdp_parse_phone(struct sdp_context* sdp)
{
	int n = 0;
	struct sdp_phone *p;

	if(sdp->p.count >= N_PHONE)
	{
		if(sdp->p.count - N_PHONE >= sdp->p.capacity)
		{
			sdp->p.ptr = (struct sdp_phone*)realloc(sdp->p.ptr, (sdp->p.capacity+8)*sizeof(struct sdp_phone));
			if(!sdp->p.ptr)
				return ENOMEM;

			sdp->p.capacity += 8;
		}

		p = &sdp->p.ptr[sdp->p.count - N_PHONE];
	}
	else
	{
		p = &sdp->p.phones[sdp->p.count];
	}

	memset(p, 0, sizeof(struct sdp_phone));
	p->phone = sdp->raw + sdp->offset;

	n = sdp_token_word(sdp, "\r\n");
	if(0 != sdp_token_crlf(sdp))
		return -1;

	trim_right(p->phone, &n);
	p->phone[n] = '\0';
	++sdp->p.count;
	return 0;
}

// RFC4566 5.7
// c=<nettype> <addrtype> <connection-address>
// A session description MUST contain either at least one "c=" field in
// each media description or a single "c=" field at the session level.
// c=IN IP4 224.2.36.42/127
// c=IN IP4 224.2.1.1/127/3
// c=IN IP6 FF15::101/3
// The slash notation for multiple addresses described above MUST NOT be
// used for IP unicast addresses
static int sdp_parse_connection(struct sdp_context* sdp)
{
	int n[3];
	struct sdp_media *m;
	struct sdp_connection *c;

	m = NULL;
	if(sdp->m.count > 0)
	{
		m = sdp_get_media(sdp, sdp->m.count-1);
		if(m->c.count >= N_CONNECTION)
		{
			if(m->c.count - N_CONNECTION >= m->c.capacity)
			{
				m->c.ptr = (struct sdp_connection*)realloc(m->c.ptr, (m->c.capacity+8)*sizeof(struct sdp_connection));
				if(!m->c.ptr)
					return ENOMEM;

				m->c.capacity += 8;
			}

			c = &m->c.ptr[m->c.count - N_CONNECTION];
		}
		else
		{
			c = &m->c.connections[m->c.count];
		}
	}
	else
	{
		c = &sdp->c;
	}

	memset(c, 0, sizeof(struct sdp_connection));
	memset(n, 0, sizeof(n));

	sdp_skip_space(sdp);
	c->network = sdp->raw + sdp->offset;
	n[0] = sdp_token_word(sdp, " \t\r\n");

	sdp_skip_space(sdp);
	c->addrtype = sdp->raw + sdp->offset;
	n[1] = sdp_token_word(sdp, " \t\r\n");

	sdp_skip_space(sdp);
	c->address = sdp->raw + sdp->offset;
	n[2] = sdp_token_word(sdp, "\r\n");
	trim_right(c->address, &n[2]);

	// check before assign '\0'
	if(0==sdp_token_crlf(sdp) && n[0]>0 && n[1]>0 && n[2]>0)
	{
		c->network[n[0]] = '\0';
		c->addrtype[n[1]] = '\0';
		c->address[n[2]] = '\0';
		if(m) ++m->c.count; // add media connection
		return 0;
	}

	return -1;
}

// RFC4566 5.8
// b=<bwtype>:<bandwidth>
// bwtype: CT/AS
// bandwidth: kilobits per second by default
// A prefix "X-" is defined for <bwtype> names.
// b=X-YZ:128
static int sdp_parse_bandwidth(struct sdp_context* sdp)
{
	int n[2];
	struct bandwidths *bs;
	struct sdp_bandwidth *b;

	if(sdp->m.count > 0)
	{
		bs = &sdp_get_media(sdp, sdp->m.count-1)->b;
	}
	else
	{
		bs = &sdp->b;
	}

	if(bs->count >= N_BANDWIDTH)
	{
		if(bs->count - N_BANDWIDTH >= bs->capacity)
		{
			bs->ptr = (struct sdp_bandwidth*)realloc(bs->ptr, (bs->capacity+8)*sizeof(struct sdp_bandwidth));
			if(!bs->ptr)
				return ENOMEM;

			bs->capacity += 8;
		}

		b = &bs->ptr[bs->count - N_BANDWIDTH];
	}
	else
	{
		b = &bs->bandwidths[bs->count];
	}

	memset(n, 0, sizeof(n));
	memset(b, 0, sizeof(struct sdp_bandwidth));

	sdp_skip_space(sdp);
	b->bwtype = sdp->raw + sdp->offset;
	n[0] = sdp_token_word(sdp, ":\r\n");
	trim_right(b->bwtype, &n[0]);

	if(':' == sdp->raw[sdp->offset])
		++sdp->offset;

	sdp_skip_space(sdp);
	b->bandwidth = sdp->raw + sdp->offset;
	n[1] = sdp_token_word(sdp, "\r\n");
	trim_right(b->bandwidth, &n[1]);

	if(0 == sdp_token_crlf(sdp))
	{
		++bs->count;
		b->bwtype[n[0]] = '\0';
		b->bandwidth[n[1]] = '\0';
		return 0;
	}
	return -1;
}

// RFC4566 5.9
// t=<start-time> <stop-time>
// If the <stop-time> is set to zero, then the session is not bounded,
// though it will not become active until after the <start-time>. If
// the <start-time> is also zero, the session is regarded as permanent.
static int sdp_parse_timing(struct sdp_context* sdp)
{
	int n[2];
	struct sdp_timing *t;

	if(sdp->t.count >= N_TIMING)
	{
		if(sdp->t.count - N_TIMING >= sdp->t.capacity)
		{
			sdp->t.ptr = (struct sdp_timing*)realloc(sdp->t.ptr, (sdp->t.capacity+8)*sizeof(struct sdp_timing));
			if(!sdp->t.ptr)
				return ENOMEM;

			sdp->t.capacity += 8;
		}

		t = &sdp->t.ptr[sdp->t.count - N_TIMING];
	}
	else
	{
		t = &sdp->t.times[sdp->t.count];
	}

	memset(n, 0, sizeof(n));
	memset(t, 0, sizeof(struct sdp_timing));

	sdp_skip_space(sdp);
	t->start = sdp->raw + sdp->offset;
	n[0] = sdp_token_word(sdp, " \t\r\n");

	sdp_skip_space(sdp);
	t->stop = sdp->raw + sdp->offset;
	n[1] = sdp_token_word(sdp, "\r\n");
	trim_right(t->stop, &n[1]);

	if(0 == sdp_token_crlf(sdp))
	{
		t->start[n[0]] = '\0';
		t->stop[n[1]] = '\0';
		++sdp->t.count;
		return 0;
	}
	return -1;
}

static int sdp_append_timing_repeat_offset(struct sdp_repeat *r, char* offset)
{
	if(r->offsets.count >= N_REPEAT_OFFSET)
	{
		if(r->offsets.count - N_REPEAT_OFFSET >= r->offsets.capacity)
		{
			r->offsets.ptr = (char**)realloc(r->offsets.ptr, (r->offsets.capacity+8)*sizeof(char*));
			if(!r->offsets.ptr)
				return ENOMEM;

			r->offsets.capacity += 8;
		}

		r->offsets.ptr[r->offsets.count - N_REPEAT_OFFSET] = offset;
	}
	else
	{
		r->offsets.offsets[r->offsets.count] = offset;
	}

	++r->offsets.count;
	return 0;
}

// RFC4566 5.10
// r=<repeat interval> <active duration> <offsets from start-time>
// t=3034423619 3042462419
// r=604800 3600 0 90000
// r=7d 1h 0 25h
static int sdp_parse_repeat(struct sdp_context* sdp)
{
	int ret;
	int n[3];
	char *offset;
	struct sdp_timing *t;
	struct sdp_repeat *r;

	assert(sdp->t.count > 0);
	if(sdp->t.count < 1)
		return -1; // repeat must after timing
	t = sdp_get_timing(sdp, sdp->t.count-1);

	if(t->r.count >= N_REPEAT)
	{
		if(t->r.count - N_REPEAT >= t->r.capacity)
		{
			t->r.ptr = (struct sdp_repeat*)realloc(t->r.ptr, (t->r.capacity+8)*sizeof(struct sdp_repeat));
			if(!t->r.ptr)
				return ENOMEM;

			t->r.capacity += 8;
		}

		r = &t->r.ptr[t->r.count - N_REPEAT];
	}
	else
	{
		r = &t->r.repeats[t->r.count];
	}

	offset = NULL;
	memset(n, 0, sizeof(n));
	memset(r, 0, sizeof(struct sdp_repeat));

	sdp_skip_space(sdp);
	r->interval = sdp->raw + sdp->offset;
	n[0] = sdp_token_word(sdp, " \t\r\n");

	sdp_skip_space(sdp);
	r->duration = sdp->raw + sdp->offset;
	n[1] = sdp_token_word(sdp, " \t\r\n");

	while(strchr(" \t", sdp->raw[sdp->offset]))
	{
		if(n[2] > 0 && offset)
		{
			offset[n[2]] = '\0';
			ret = sdp_append_timing_repeat_offset(r, offset);
			if(0 != ret)
				return ret;
		}

		sdp_skip_space(sdp);
		offset = sdp->raw + sdp->offset;
		n[2] = sdp_token_word(sdp, " \t\r\n");
	}

	if(0 == sdp_token_crlf(sdp))
	{
		r->interval[n[0]] = '\0';
		r->duration[n[1]] = '\0';
		if(n[2] > 0 && offset)
		{
			offset[n[2]] = '\0';
			ret = sdp_append_timing_repeat_offset(r, offset);
			if(0 != ret)
				return ret;
		}
		++t->r.count;
		return 0;
	}
	return -1;
}

// RFC4566 5.11
// z=<adjustment time> <offset> <adjustment time> <offset> ....
// z=2882844526 -1h 2898848070 0
static int sdp_parse_timezone(struct sdp_context* sdp)
{
	int n[2];
	char *time, *offset;
	struct sdp_timing *t;
	struct sdp_timezone *z;

	assert(sdp->t.count > 0);
	if(sdp->t.count < 1)
		return -1; // timezone must after timing
	t = sdp_get_timing(sdp, sdp->t.count-1);

	do
	{
		sdp_skip_space(sdp);
		time = sdp->raw + sdp->offset;
		n[0] = sdp_token_word(sdp, " \t\r\n");

		sdp_skip_space(sdp);
		offset = sdp->raw + sdp->offset;
		n[1] = sdp_token_word(sdp, " \t\r\n");

		if(n[0] < 1 || n[1] < 1)
			break;

		if(t->z.count >= N_TIMEZONE)
		{
			if(t->z.count - N_TIMEZONE >= t->z.capacity)
			{
				t->z.ptr = (struct sdp_timezone*)realloc(t->z.ptr, (t->z.capacity+8)*sizeof(struct sdp_timezone));
				if(!t->z.ptr)
					return ENOMEM;

				t->z.capacity += 8;
			}

			z = &t->z.ptr[t->r.count - N_TIMEZONE];
		}
		else
		{
			z = &t->z.timezones[t->r.count];
		}

		z->time = time;
		z->offset = offset;
		++t->z.count;

	} while(n[0] > 0 && n[1] > 0);

	return sdp_token_crlf(sdp);
}

// RFC4566 5.12
// k=<method>
// k=<method>:<encryption key>
// k=clear:<encryption key>
// k=base64:<encoded encryption key>
// k=uri:<URI to obtain key>
// k=prompt
// A key field is permitted before the first media entry (in which case
// it applies to all media in the session), or for each media entry as required.
static int sdp_parse_encryption(struct sdp_context* sdp)
{
	int n[2];
	struct sdp_encryption *k;

	if(sdp->m.count > 0)
	{
		k = &sdp_get_media(sdp, sdp->m.count-1)->k;
	}
	else
	{
		k = &sdp->k;
	}
	memset(k, 0, sizeof(struct sdp_encryption));

	sdp_skip_space(sdp);
	k->method = sdp->raw + sdp->offset;
	n[0] = sdp_token_word(sdp, ":\r\n");
	trim_right(k->method, &n[0]);

	if(':' == sdp->raw[sdp->offset])
		++sdp->offset; // skip ':'

	sdp_skip_space(sdp);
	k->key = sdp->raw + sdp->offset;
	n[1] = sdp_token_word(sdp, "\r\n");
	trim_right(k->key, &n[1]);

	if(0 == sdp_token_crlf(sdp))
	{
		k->method[n[0]] = '\0';
		k->key[n[1]] = '\0';
		return 0;
	}
	return -1;
}

// RFC4566 5.13
// a=<attribute>
// a=<attribute>:<value>
// a=cat:<category>
// a=keywds:<keywords>
// a=tool:<name and version of tool>
// a=ptime:<packet time>
// a=maxptime:<maximum packet time>
// a=rtpmap:<payload type> <encoding name>/<clock rate> [/<encoding parameters>]
// a=recvonly
// a=sendrecv
// a=sendonly
// a=inactive
// a=orient:<orientation>
// a=type:<conference type>
// a=charset:<character set>
// a=sdplang:<language tag>
// a=lang:<language tag>
// a=framerate:<frame rate>
// a=quality:<quality>
// a=fmtp:<format> <format specific parameters>
static int sdp_parse_attribute(struct sdp_context* sdp)
{
	int n[2];
	struct attributes *as;
	struct sdp_attribute *a;

	if(sdp->m.count > 0)
	{
		as = &sdp_get_media(sdp, sdp->m.count-1)->a;
	}
	else
	{
		as = &sdp->a;
	}

	if(as->count >= N_ATTRIBUTE)
	{
		if(as->count - N_ATTRIBUTE >= as->capacity)
		{
			as->ptr = (struct sdp_attribute*)realloc(as->ptr, (as->capacity+8)*sizeof(struct sdp_attribute));
			if(!as->ptr)
				return ENOMEM;

			as->capacity += 8;
		}

		a = &as->ptr[as->count - N_ATTRIBUTE];
	}
	else
	{
		a = &as->attrs[as->count];
	}

	memset(a, 0, sizeof(struct sdp_attribute));

	sdp_skip_space(sdp);
	a->name = sdp->raw + sdp->offset;
	n[0] = sdp_token_word(sdp, ":\r\n");
	trim_right(a->name, &n[0]);

	if(':' == sdp->raw[sdp->offset])
		++sdp->offset; // skip ':'

	sdp_skip_space(sdp);
	a->value = sdp->raw + sdp->offset;
	n[1] = sdp_token_word(sdp, "\r\n");
	trim_right(a->value, &n[1]);

	if(0 == sdp_token_crlf(sdp))
	{
		++as->count;
		a->name[n[0]] = '\0';
		a->value[n[1]] = '\0';
		return 0;
	}
	return -1;
}

static int sdp_append_media_format(struct sdp_media *m, char* fmt)
{
	if(m->fmt.count >= N_MEDIA_FORMAT)
	{
		if(m->fmt.count - N_MEDIA_FORMAT >= m->fmt.capacity)
		{
			m->fmt.ptr = (char**)realloc(m->fmt.ptr, (m->fmt.capacity+8)*sizeof(char*));
			if(!m->fmt.ptr)
				return ENOMEM;

			m->fmt.capacity += 8;
		}

		m->fmt.ptr[m->fmt.count - N_MEDIA_FORMAT] = fmt;
	}
	else
	{
		m->fmt.formats[m->fmt.count] = fmt;
	}

	++m->fmt.count;
	return 0;
}

// RFC4566 5.14
// m=<media> <port> <proto> <fmt> ...
// media: audio/video/text/application/message
// proto: udp, RTP/AVP, RTP/SAVP
// m=<media> <port>/<number of ports> <proto> <fmt> ...
// m=video 49170/2 RTP/AVP 31
// c=IN IP4 224.2.1.1/127/2
// m=video 49170/2 RTP/AVP 31
static int sdp_parse_media(struct sdp_context* sdp)
{
	int ret;
	int n[4];
	char *fmt;
	struct sdp_media *m;

	if(sdp->m.count >= N_MEDIA)
	{
		if(sdp->m.count - N_MEDIA >= sdp->m.capacity)
		{
			sdp->m.ptr = (struct sdp_media*)realloc(sdp->m.ptr, (sdp->m.capacity+8)*sizeof(struct sdp_media));
			if(!sdp->m.ptr)
				return ENOMEM;

			sdp->m.capacity += 8;
		}

		m = &sdp->m.ptr[sdp->m.count - N_MEDIA];
	}
	else
	{
		m = &sdp->m.medias[sdp->m.count];
	}

	memset(m, 0, sizeof(struct sdp_media));

	sdp_skip_space(sdp);
	m->media = sdp->raw + sdp->offset;
	n[0] = sdp_token_word(sdp, " \t\r\n");

	sdp_skip_space(sdp);
	m->port = sdp->raw + sdp->offset;
	n[1] = sdp_token_word(sdp, " \t\r\n");

	sdp_skip_space(sdp);
	m->proto = sdp->raw + sdp->offset;
	n[2] = sdp_token_word(sdp, " \t\r\n");

	sdp_skip_space(sdp);
	fmt = sdp->raw + sdp->offset;
	n[3] = sdp_token_word(sdp, " \t\r\n");

	while(' ' == fmt[n[3]] || '\t' == fmt[n[3]])
	{
		fmt[n[3]] = '\0';
		ret = sdp_append_media_format(m, fmt);
		if(0 != ret)
			return ret;
	
		sdp_skip_space(sdp);
		fmt = sdp->raw + sdp->offset;
		n[3] = sdp_token_word(sdp, " \t\r\n");
	}

	if(0 == sdp_token_crlf(sdp))
	{
		m->media[n[0]] = '\0';
		m->port[n[1]] = '\0';
		m->proto[n[2]] = '\0';
		if(n[3] > 0)
		{
			fmt[n[3]] = '\0';
			ret = sdp_append_media_format(m, fmt);
			if(0 != ret)
				return ret;
		}

		++sdp->m.count;
		return 0;
	}
	return -1;
}

void* sdp_create()
{
	struct sdp_context *sdp;
	sdp = (struct sdp_context*)malloc(sizeof(struct sdp_context));
	if( !sdp )
		return NULL;

	memset(sdp, 0, sizeof(struct sdp_context));
	return sdp;
}

void sdp_destroy(void* p)
{
	int i;
	struct sdp_media *m;
	struct sdp_timing *t;
	struct sdp_context *sdp;
	sdp = (struct sdp_context*)p;

	if(sdp->e.count > N_EMAIL)
		free(sdp->e.ptr);

	if(sdp->p.count > N_PHONE)
		free(sdp->p.ptr);

	if(sdp->b.count > N_BANDWIDTH)
		free(sdp->b.ptr);

	for(i = 0; i < sdp->t.count; i++)
	{
		t = sdp_get_timing(sdp, i);
		if(t->r.count > N_REPEAT)
			free(t->r.ptr);

		if(t->z.count > N_TIMEZONE)
			free(t->z.ptr);
	}

	if(sdp->t.count > N_TIMING)
		free(sdp->t.ptr);

	if(sdp->a.count > N_ATTRIBUTE)
		free(sdp->a.ptr);

	for(i = 0; i < sdp->m.count; i++)
	{
		m = sdp_get_media(sdp, i);
		if(m->fmt.count > N_MEDIA_FORMAT)
			free(m->fmt.ptr);

		if(m->a.count > N_ATTRIBUTE)
			free(m->a.ptr);

		if(m->c.count > N_CONNECTION)
			free(m->c.ptr);
	}

	if(sdp->m.count > N_MEDIA)
		free(sdp->m.ptr);

	if(sdp->raw)
		free(sdp->raw);

	free(sdp);
}

void* sdp_parse(const char* s)
{
	int r;
	char c;
	struct sdp_context *sdp;

	assert(s);
	sdp = (struct sdp_context*)sdp_create();
	if(!sdp)
		return NULL;

	sdp->raw = strdup(s);
	if(!sdp->raw)
		goto parse_failed;

	while(sdp->raw[sdp->offset] && !strchr("\r\n", sdp->raw[sdp->offset]))
	{
		sdp_skip_space(sdp);
		c = sdp->raw[sdp->offset++];

		sdp_skip_space(sdp);
		if('=' != sdp->raw[sdp->offset++])
			goto parse_failed;

		sdp_skip_space(sdp);

		r = 0;
		switch(c)
		{
		case 'v':
			r = sdp_parse_version(sdp);
			break;

		case 'o':
			r = sdp_parse_origin(sdp);
			break;

		case 's':
			r = sdp_parse_session(sdp);
			break;

		case 'i':
			r = sdp_parse_information(sdp);
			break;

		case 'u':
			r = sdp_parse_uri(sdp);
			break;

		case 'e':
			r = sdp_parse_email(sdp);
			break;

		case 'p':
			r = sdp_parse_phone(sdp);
			break;

		case 'c':
			r = sdp_parse_connection(sdp);
			break;

		case 'b':
			r = sdp_parse_bandwidth(sdp);
			break;

		case 't':
			r = sdp_parse_timing(sdp);
			break;

		case 'r':
			r = sdp_parse_repeat(sdp);
			break;

		case 'z':
			r = sdp_parse_timezone(sdp);
			break;

		case 'k':
			r = sdp_parse_encryption(sdp);
			break;

		case 'a':
			r = sdp_parse_attribute(sdp);
			break;

		case 'm':
			r = sdp_parse_media(sdp);
			break;

		default:
			assert(0); // unknown sdp
			while(*s && '\n' != *s)
				++s; // skip line
		}

		if(0 != r)
			goto parse_failed;
	}

	return sdp;

parse_failed:
	sdp_destroy(sdp);
	return NULL;
}

int sdp_version_get(void* sdp)
{
	struct sdp_context *ctx;
	ctx = (struct sdp_context*)sdp;
	return ctx->v;
}

//void sdp_version_set(void* sdp, int version)
//{
//	struct sdp_context *ctx;
//	ctx = (struct sdp_context*)sdp;
//	ctx->v = version;
//}

int sdp_origin_get(void* sdp, const char **username, const char** session, const char** version, const char** network, const char** addrtype, const char** address)
{
	struct sdp_context *ctx;
	ctx = (struct sdp_context*)sdp;
	if(ctx->o.username && ctx->o.session && ctx->o.session_version 
		&& ctx->o.c.network && ctx->o.c.addrtype && ctx->o.c.address)
	{
		*username = ctx->o.username;
		*session = ctx->o.session;
		*version = ctx->o.session_version;
		*network = ctx->o.c.network;
		*addrtype = ctx->o.c.addrtype;
		*address = ctx->o.c.address;
		return 0;
	}
	return -1;
}

int sdp_origin_get_network(void* sdp)
{
	struct sdp_context *ctx;
	ctx = (struct sdp_context*)sdp;
	if(0 == stricmp("IN", ctx->o.c.network))
		return SDP_C_NETWORK_IN;
	return SDP_C_NETWORK_UNKNOWN;
}

int sdp_origin_get_addrtype(void* sdp)
{
	struct sdp_context *ctx;
	ctx = (struct sdp_context*)sdp;
	if(0 == stricmp("IP4", ctx->o.c.addrtype))
		return SDP_C_ADDRESS_IP4;
	if(0 == stricmp("IP6", ctx->o.c.addrtype))
		return SDP_C_ADDRESS_IP6;
	return SDP_C_ADDRESS_UNKNOWN;
}

const char* sdp_session_get_name(void* sdp)
{
	struct sdp_context *ctx;
	ctx = (struct sdp_context*)sdp;
	return ctx->s;
}

const char* sdp_session_get_information(void* sdp)
{
	struct sdp_context *ctx;
	ctx = (struct sdp_context*)sdp;
	return ctx->i;
}

const char* sdp_uri_get(void* sdp)
{
	struct sdp_context *ctx;
	ctx = (struct sdp_context*)sdp;
	return ctx->u;
}

int sdp_email_count(void* sdp)
{
	struct sdp_context *ctx;
	ctx = (struct sdp_context*)sdp;
	return ctx->e.count;
}

const char* sdp_email_get(void* sdp, int idx)
{
	struct sdp_context *ctx;
	ctx = (struct sdp_context*)sdp;
	if(idx >= ctx->e.count || idx < 0)
		return NULL;

	return idx < N_EMAIL ? ctx->e.emails[idx].email : ctx->e.ptr[idx - N_EMAIL].email;
}

int sdp_phone_count(void* sdp)
{
	struct sdp_context *ctx;
	ctx = (struct sdp_context*)sdp;
	return ctx->p.count;
}

const char* sdp_phone_get(void* sdp, int idx)
{
	struct sdp_context *ctx;
	ctx = (struct sdp_context*)sdp;
	if(idx >= ctx->p.count || idx < 0)
		return NULL;

	return idx < N_PHONE ? ctx->p.phones[idx].phone : ctx->p.ptr[idx - N_PHONE].phone;
}

int sdp_connection_get(void* sdp, const char** network, const char** addrtype, const char** address)
{
	struct sdp_context *ctx;
	ctx = (struct sdp_context*)sdp;
	if(ctx->c.network && ctx->c.addrtype && ctx->c.address)
	{
		*network = ctx->c.network;
		*addrtype = ctx->c.addrtype;
		*address = ctx->c.address;
		return 0;
	}
	return -1;
}

int sdp_connection_get_address(void* sdp, char* ip, int bytes)
{
	const char* p;
	struct sdp_context *ctx;
	ctx = (struct sdp_context*)sdp;

	if(ctx->c.address && bytes > 0)
	{
		p = ctx->c.address;
		while(*p && '/' != *p && bytes > 1)
		{
			*ip++ = *p;
			--bytes;
		}

		if(0 == *p || '/' == *p)
		{
			*ip = '\0';
			return 0;
		}
	}
	return -1;
}

int sdp_connection_get_network(void* sdp)
{
	struct sdp_context *ctx;
	ctx = (struct sdp_context*)sdp;
	if(0 == stricmp("IN", ctx->c.network))
		return SDP_C_NETWORK_IN;
	return SDP_C_NETWORK_UNKNOWN;
}

int sdp_connection_get_addrtype(void* sdp)
{
	struct sdp_context *ctx;
	ctx = (struct sdp_context*)sdp;
	if(0 == stricmp("IP4", ctx->c.addrtype))
		return SDP_C_ADDRESS_IP4;
	if(0 == stricmp("IP6", ctx->c.addrtype))
		return SDP_C_ADDRESS_IP6;
	return SDP_C_ADDRESS_UNKNOWN;
}

int sdp_bandwidth_count(void* sdp)
{
	struct sdp_context *ctx;
	ctx = (struct sdp_context*)sdp;
	return ctx->b.count;
}

const char* sdp_bandwidth_get_type(void* sdp, int idx)
{
	struct sdp_context *ctx;
	ctx = (struct sdp_context*)sdp;
	if(idx >= ctx->b.count || idx < 0)
		return NULL;
	return idx < N_BANDWIDTH ? ctx->b.bandwidths[idx].bwtype : ctx->b.ptr[idx - N_BANDWIDTH].bwtype;
}

int sdp_bandwidth_get_value(void* sdp, int idx)
{
	const char* b;
	struct sdp_context *ctx;
	ctx = (struct sdp_context*)sdp;
	if(idx >= ctx->b.count || idx < 0)
		return -1;

	b = idx < N_BANDWIDTH ? ctx->b.bandwidths[idx].bandwidth : ctx->b.ptr[idx - N_BANDWIDTH].bandwidth;
	return atoi(b);
}

int sdp_timing_count(void* sdp)
{
	struct sdp_context *ctx;
	ctx = (struct sdp_context*)sdp;
	return ctx->t.count;
}

int sdp_media_count(void* sdp)
{
	struct sdp_context *ctx;
	ctx = (struct sdp_context*)sdp;
	return ctx->m.count;
}

int sdp_media_port(void* sdp, int media, int *port)
{
	struct sdp_media *m;
	struct sdp_context *ctx;
	ctx = (struct sdp_context*)sdp;
	m = sdp_get_media(sdp, media);
	if(m && port)
	{
		*port = atoi(m->port);
		return 0;
	}
	return -1;
}

int sdp_media_get_connection_address(void* sdp, int media, char* ip, int bytes)
{
	const char* p;
	struct sdp_media *m;
	struct sdp_context *ctx;
	struct sdp_connection *conn;
	ctx = (struct sdp_context*)sdp;
	m = sdp_get_media(sdp, media);
	if(m && m->c.count > 0)
		conn = &m->c.connections[0];
	else
		conn = &ctx->c;

	if(conn->address && bytes > 0)
	{
		p = conn->address;
		while(*p && '/' != *p && bytes > 1)
		{
			*ip++ = *p;
			--bytes;
		}

		if(0 == *p || '/' == *p)
		{
			*ip = '\0';
			return 0;
		}
	}
	return -1;
}

int sdp_media_get_connection_network(void* sdp, int media)
{
	struct sdp_media *m;
	struct sdp_context *ctx;
	struct sdp_connection *conn;
	ctx = (struct sdp_context*)sdp;
	m = sdp_get_media(sdp, media);
	if(m && m->c.count > 0)
		conn = &m->c.connections[0];
	else
		conn = &ctx->c;

	if(conn->network)
	{
		if(0 == stricmp("IN", conn->network))
			return SDP_C_NETWORK_IN;
	}
	return SDP_C_NETWORK_UNKNOWN;
}

int sdp_media_get_connection_addrtype(void* sdp, int media)
{
	struct sdp_media *m;
	struct sdp_context *ctx;
	struct sdp_connection *conn;
	ctx = (struct sdp_context*)sdp;
	m = sdp_get_media(sdp, media);
	if(m && m->c.count > 0)
		conn = &m->c.connections[0];
	else
		conn = &ctx->c;

	if(conn->addrtype)
	{
		if(0 == stricmp("IP4", conn->addrtype))
			return SDP_C_ADDRESS_IP4;
		if(0 == stricmp("IP6", conn->addrtype))
			return SDP_C_ADDRESS_IP6;
	}
	return SDP_C_ADDRESS_UNKNOWN;
}

const char* sdp_media_attribute_find(void* sdp, int media, const char* name)
{
	int i;
	struct sdp_media *m;
	struct sdp_context *ctx;
	struct sdp_attribute *attr;
	ctx = (struct sdp_context*)sdp;
	m = sdp_get_media(sdp, media);
	for(i = 0; name && m && i < m->a.count; i++)
	{
		if(i < N_ATTRIBUTE)
			attr = m->a.attrs + i;
		else
			attr = m->a.ptr + i - N_ATTRIBUTE;

		if(attr->name && 0==strcmp(attr->name, name))
			return attr->value;
	}

	return NULL;
}

int sdp_media_bandwidth_count(void* sdp, int media)
{
	struct sdp_media *m;
	struct sdp_context *ctx;
	ctx = (struct sdp_context*)sdp;
	m = sdp_get_media(sdp, media);
	if(!m)
		return 0;
	return m->b.count;
}

const char* sdp_media_bandwidth_get_type(void* sdp, int media, int idx)
{
	struct sdp_media *m;
	struct sdp_context *ctx;
	ctx = (struct sdp_context*)sdp;
	m = sdp_get_media(sdp, media);
	if(!m)
		return NULL;

	if(idx >= m->b.count || idx < 0)
		return NULL;
	return idx < N_BANDWIDTH ? m->b.bandwidths[idx].bwtype : m->b.ptr[idx - N_BANDWIDTH].bwtype;
}

int sdp_media_bandwidth_get_value(void* sdp, int media, int idx)
{
	const char* b;
	struct sdp_media *m;
	struct sdp_context *ctx;
	ctx = (struct sdp_context*)sdp;
	m = sdp_get_media(sdp, media);
	if(!m)
		return -1;

	if(idx >= m->b.count || idx < 0)
		return -1;

	b = idx < N_BANDWIDTH ? m->b.bandwidths[idx].bandwidth : m->b.ptr[idx - N_BANDWIDTH].bandwidth;
	return atoi(b);
}

int sdp_attribute_count(void* sdp)
{
	struct sdp_context *ctx;
	ctx = (struct sdp_context*)sdp;
	return ctx->a.count;
}

int sdp_attribute_get(void* sdp, int idx, const char** name, const char** value)
{
	struct sdp_context *ctx;
	struct sdp_attribute *attr;
	ctx = (struct sdp_context*)sdp;
	if(idx < 0 || idx > ctx->a.count)
		return -1; // not found

	if(idx < N_ATTRIBUTE)
		attr = ctx->a.attrs + idx;
	else
		attr = ctx->a.ptr + idx - N_ATTRIBUTE;

	*name = attr->name;
	*value = attr->value;
	return 0;
}

const char* sdp_attribute_find(void* sdp, const char* name)
{
	int i;
	struct sdp_context *ctx;
	struct sdp_attribute *attr;
	ctx = (struct sdp_context*)sdp;
	for(i = 0; name && i < ctx->a.count; i++)
	{
		if(i < N_ATTRIBUTE)
			attr = ctx->a.attrs + i;
		else
			attr = ctx->a.ptr + i - N_ATTRIBUTE;

		if(attr->name && 0==strcmp(attr->name, name))
			return attr->value;
	}

	return NULL;
}