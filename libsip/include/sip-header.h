#ifndef _sip_header_h_
#define _sip_header_h_

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include "cstring.h"
#include "darray.h"

#if defined(__cplusplus)
extern "C" {
#endif

// 10.2.6 Discovering a Registrar (p62)
#define SIP_MULTICAST_HOST "sip.mcast.net"
#define SIP_MULTICAST_ADDRESS "224.0.1.75"

#define DARRAY_DECLARE(name)				\
	struct name##s_t						\
	{										\
		struct darray_t arr;				\
	};										\
											\
	void name##s_init(struct name##s_t* p);	\
	void name##s_free(struct name##s_t* p);	\
	int name##s_count(const struct name##s_t* p);	\
	int name##s_push(struct name##s_t* p, struct name##_t* item); \
	struct name##_t* name##s_get(const struct name##s_t* p, int index);

#define DARRAY_IMPLEMENT(name, N)			\
	static inline void name##s_arrfree(struct darray_t *arr)	\
	{										\
		int i;								\
		struct name##s_t* p;				\
		p = (struct name##s_t*)(((char*)arr) - offsetof(struct name##s_t, arr)); \
		for(i = 0; i < darray_count(arr); i++) \
			name##_params_free(name##s_get(p, i)); \
		if(arr && arr->elements)			\
			free(arr->elements);			\
	}										\
											\
	static inline void* name##s_arralloc(struct darray_t *arr, size_t size) \
	{										\
		return realloc(arr->elements, size);\
	}										\
											\
	void name##s_init(struct name##s_t* p)	\
	{										\
		p->arr.free = name##s_arrfree;		\
		p->arr.alloc = name##s_arralloc;	\
		darray_init(&p->arr, sizeof(struct name##_t), N); \
	}										\
											\
	void name##s_free(struct name##s_t* p)	\
	{										\
		darray_free(&p->arr);				\
	}										\
											\
	int name##s_push(struct name##s_t* p, struct name##_t* item)	\
	{										\
		return darray_insert(&p->arr, -1, item);	\
	}										\
											\
	struct name##_t* name##s_get(const struct name##s_t* p, int index)	\
	{										\
		return (struct name##_t*)darray_get(&((struct name##s_t*)p)->arr, index);	\
	}										\
											\
	int name##s_count(const struct name##s_t* p)	\
	{										\
		return darray_count(&p->arr);		\
	}


struct sip_param_t
{
	struct cstring_t name;
	struct cstring_t value;
};
DARRAY_DECLARE(sip_param);

struct sip_uri_t
{
	struct cstring_t scheme;
	struct cstring_t host; // userinfo@host:port
	
	struct sip_params_t parameters;
	struct cstring_t transport; // udp/tcp/sctp/tls/other
	struct cstring_t method;
	struct cstring_t maddr; // the server address to be contacted for this user, overriding any address derived from the host field
	struct cstring_t user; // phone/ip
	int ttl;
	int lr;
	int rport; // 0-not found, -1-no-value, other-value

	struct sip_params_t headers;
};
DARRAY_DECLARE(sip_uri);

struct sip_requestline_t
{
	struct cstring_t method;
	struct sip_uri_t uri;
};

struct sip_statusline_t
{
	int code;
	int verminor, vermajor;
	char protocol[64];
	struct cstring_t reason;
};

struct sip_contact_t
{
	struct sip_uri_t uri;
	struct cstring_t nickname;

	// parameters
	struct cstring_t tag; // TO/FROM
	double q; // c-p-q
	int64_t expires; // delta-seconds, default 3600
	struct sip_params_t params; // include tag/q/expires
};
DARRAY_DECLARE(sip_contact);

struct sip_via_t
{
	struct cstring_t protocol;
	struct cstring_t version;
	struct cstring_t transport;
	struct cstring_t host; // sent-by host:port

	// parameters
	struct cstring_t branch; // token
	struct cstring_t maddr; // host
	struct cstring_t received; // IPv4address / IPv6address
	int ttl; // 0-255
	int rport; // 0-not found, -1-no-value, other-value
	struct sip_params_t params; // include branch/maddr/received/ttl/rport
};
DARRAY_DECLARE(sip_via);

struct sip_cseq_t
{
	uint32_t id;
	struct cstring_t method;
};

struct sip_substate_t
{
	struct cstring_t state;

	// parameters
	struct cstring_t reason;
	uint32_t expires; // expires
	uint32_t retry; // retry-after
	struct sip_params_t params; // include reason/expires/retry
};

int sip_header_param(const char* s, const char* end, struct sip_param_t* param);
int sip_header_params(char sep, const char* s, const char* end, struct sip_params_t* params);
int sip_param_write(const struct sip_param_t* param, char* data, const char* end);
int sip_params_write(const struct sip_params_t* params, char* data, const char* end, char sep);
const struct sip_param_t* sip_params_find(const struct sip_params_t* params, const char* name, int bytes);
const struct cstring_t* sip_params_find_string(const struct sip_params_t* params, const char* name, int bytes);
int sip_params_find_int(const struct sip_params_t* params, const char* name, int bytes, int* value);
int sip_params_find_int64(const struct sip_params_t* params, const char* name, int bytes, int64_t* value);
int sip_params_find_double(const struct sip_params_t* params, const char* name, int bytes, double* value);

/// @return 0-ok, other-error
int sip_header_cseq(const char* s, const char* end, struct sip_cseq_t* cseq);
/// @return write length, >0-ok, <0-error
int sip_cseq_write(const struct sip_cseq_t* cseq, char* data, const char* end);

int sip_header_uri(const char* s, const char* end, struct sip_uri_t* uri);
int sip_uri_write(const struct sip_uri_t* uri, char* data, const char* end);
int sip_uri_equal(const struct sip_uri_t* l, const struct sip_uri_t* r);
int sip_uri_username(const struct sip_uri_t* uri, struct cstring_t* user);
int sip_request_uri_write(const struct sip_uri_t* uri, char* data, const char* end);

int sip_header_via(const char* s, const char* end, struct sip_via_t* via);
int sip_header_vias(const char* s, const char* end, struct sip_vias_t* vias);
int sip_via_write(const struct sip_via_t* via, char* data, const char* end);
const struct cstring_t* sip_vias_top_branch(const struct sip_vias_t* vias);

int sip_header_contact(const char* s, const char* end, struct sip_contact_t* contact);
int sip_header_contacts(const char* s, const char* end, struct sip_contacts_t* contacts);
int sip_contact_write(const struct sip_contact_t* contact, char* data, const char* end);
int sip_contacts_match_any(const struct sip_contacts_t* contacts);

char* cstring_clone(char* ptr, const char* end, struct cstring_t* clone, const char* s, size_t n);
char* sip_uri_clone(char* ptr, const char* end, struct sip_uri_t* clone, const struct sip_uri_t* uri);
char* sip_via_clone(char* ptr, const char* end, struct sip_via_t* clone, const struct sip_via_t* via);
char* sip_contact_clone(char* ptr, const char* end, struct sip_contact_t* clone, const struct sip_contact_t* contact);

void sip_uri_params_free(struct sip_uri_t* uri);
void sip_via_params_free(struct sip_via_t* via);
void sip_contact_params_free(struct sip_contact_t* contact);

/// @return 0-ok, other-error
int sip_header_substate(const char* s, const char* end, struct sip_substate_t* substate);
/// @return write length, >0-ok, <0-error
int sip_substate_write(const struct sip_substate_t* substate, char* data, const char* end);

#if defined(__cplusplus)
}
#endif
#endif /* !_sip_header_h_ */
