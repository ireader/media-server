#ifndef _sip_header_h_
#define _sip_header_h_

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include "cstring.h"
#include "darray.h"

// 10.2.6 Discovering a Registrar (p62)
#define SIP_MULTICAST_HOST "sip.mcast.net"
#define SIP_MULTICAST_ADDRESS "224.0.1.75"

#define DARRAY_DECLARE(name, N)				\
	struct name##s_t						\
	{										\
		struct darray_t arr;				\
		struct name##_t ptr[N];				\
	};										\
											\
	void name##s_init(struct name##s_t* p);	\
	void name##s_free(struct name##s_t* p);	\
	int name##s_count(const struct name##s_t* p);	\
	int name##s_push(struct name##s_t* p, struct name##_t* item); \
	struct name##_t* name##s_get(struct name##s_t* p, int index);

#define DARRAY_IMPLEMENT(name)				\
	static inline void name##s_arrfree(struct darray_t *arr)	\
	{										\
		struct name##s_t* p;				\
		p = (struct name##s_t*)(((char*)arr) - offsetof(struct name##s_t, arr)); \
		if(p->arr.elements != p->ptr)		\
			free(p->arr.elements);			\
	}										\
											\
	static inline void* name##s_arralloc(struct darray_t *arr, size_t size) \
	{										\
		void* ptr;							\
		struct name##s_t* p;				\
		p = (struct name##s_t*)(((char*)arr) - offsetof(struct name##s_t, arr));	\
		if(size <= sizeof(p->ptr)) return p->ptr;									\
		ptr = realloc(p->ptr == p->arr.elements ? NULL : p->arr.elements, size);	\
		if (ptr && p->ptr == p->arr.elements)										\
			memcpy(ptr, p->ptr, sizeof(p->ptr));									\
		return ptr;																	\
	}										\
											\
	void name##s_init(struct name##s_t* p)	\
	{										\
		p->arr.free = name##s_arrfree;		\
		p->arr.alloc = name##s_arralloc;	\
		darray_init(&p->arr, sizeof(struct name##_t), sizeof(p->ptr)/sizeof(p->ptr[0])); \
	}										\
											\
	void name##s_free(struct name##s_t* p)	\
	{										\
		darray_free(&p->arr);				\
	}										\
											\
	int name##s_push(struct name##s_t* p, struct name##_t* item)	\
	{										\
		return darray_push_back(&p->arr, item, 1);	\
	}										\
											\
	struct name##_t* name##s_get(struct name##s_t* p, int index)	\
	{										\
		return (struct name##_t*)darray_get(&p->arr, index);	\
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
DARRAY_DECLARE(sip_param, 5);

struct sip_uri_t
{
	struct cstring_t scheme;
	struct cstring_t host; // userinfo@host:port
	
	struct sip_params_t parameters;
	struct cstring_t transport; // udp/tcp/sctp/tls/other
	struct cstring_t method;
	struct cstring_t maddr;
	struct cstring_t user; // phone/ip
	int ttl;
	int lr;

	struct sip_params_t headers;
};
DARRAY_DECLARE(sip_uri, 3);

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
DARRAY_DECLARE(sip_contact, 3);

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
	struct sip_params_t params; // include branch/maddr/received/ttl
};
DARRAY_DECLARE(sip_via, 8);

struct sip_cseq_t
{
	uint32_t id;
	struct cstring_t method;
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

int sip_uri_free(struct sip_uri_t* uri);
int sip_uri_clone(struct sip_uri_t* clone, const struct sip_uri_t* uri);
int sip_header_uri(const char* s, const char* end, struct sip_uri_t* uri);
int sip_uri_write(const struct sip_uri_t* uri, char* data, const char* end);
int sip_uri_equal(const struct sip_uri_t* l, const struct sip_uri_t* r);

int sip_via_free(struct sip_via_t* via);
int sip_via_clone(struct sip_via_t* clone, const struct sip_via_t* via);
int sip_header_via(const char* s, const char* end, struct sip_via_t* via);
int sip_header_vias(const char* s, const char* end, struct sip_vias_t* vias);
int sip_via_write(const struct sip_via_t* via, char* data, const char* end);
const struct cstring_t* sip_vias_top_branch(const struct sip_vias_t* vias);

int sip_contact_free(struct sip_contact_t* contact);
int sip_contact_clone(struct sip_contact_t* clone, const struct sip_contact_t* contact);
int sip_header_contact(const char* s, const char* end, struct sip_contact_t* contact);
int sip_header_contacts(const char* s, const char* end, struct sip_contacts_t* contacts);
int sip_contact_write(const struct sip_contact_t* contact, char* data, const char* end);
int sip_contacts_match_any(const struct sip_contacts_t* contacts);

int cstring_free(struct cstring_t* s);
int cstring_clone(struct cstring_t* clone, const struct cstring_t* s);

#endif /* !_sip_header_h_ */
