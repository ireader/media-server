#ifndef _sip_header_h_
#define _sip_header_h_

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include "cstring.h"
#include "darray.h"

#define DARRAY_DECLARE(name, N)				\
	struct name##s_t						\
	{										\
		struct darray_t arr;				\
		struct name##_t ptr[N];				\
	};										\
											\
	void name##s_init(struct name##s_t* p);	\
	void name##s_free(struct name##s_t* p);	\
	int name##s_count(struct name##s_t* p);	\
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
DARRAY_DECLARE(sip_param, 2);

struct sip_uri_t
{
	struct cstring_t scheme;
	struct cstring_t host; // userinfo@host:port
	struct cstring_t parameters;
	struct cstring_t headers;
};
DARRAY_DECLARE(sip_uri, 1);

struct sip_requestline_t
{
	struct cstring_t method;
	struct sip_uri_t uri;
};

struct sip_statusline_t
{
	int code;
	struct cstring_t version;
	struct cstring_t reason;
};

struct sip_contact_t
{
	struct sip_uri_t uri;
	struct cstring_t nickname;
	struct sip_params_t params;
};
DARRAY_DECLARE(sip_contact, 1);

struct sip_via_t
{
	struct cstring_t protocol;
	struct cstring_t version;
	struct cstring_t transport;
	struct cstring_t host; // host:port
	struct sip_params_t params;
};
DARRAY_DECLARE(sip_via, 1);

struct sip_cseq_t
{
	uint32_t id;
	struct cstring_t method;
};

int sip_header_param(const char* s, const char* end, struct sip_param_t* param);
int sip_param_write(const struct sip_param_t* param, char* data, const char* end);
int sip_params_write(const struct sip_params_t* params, char* data, const char* end);
const struct sip_param_t* sip_params_find(const struct sip_params_t* params, const char* name);
const struct cstring_t* sip_params_find_string(const struct sip_params_t* params, const char* name);
int sip_params_find_int(const struct sip_params_t* params, const char* name, int* value);
int sip_params_find_int64(const struct sip_params_t* params, const char* name, int64_t* value);
int sip_params_find_double(const struct sip_params_t* params, const char* name, double* value);

/// @return 0-ok, other-error
int sip_header_uri(const char* s, const char* end, struct sip_uri_t* uri);
int sip_header_via(const char* s, const char* end, struct sip_vias_t* vias);
int sip_header_cseq(const char* s, const char* end, struct sip_cseq_t* cseq);
int sip_header_contact(const char* s, const char* end, struct sip_contacts_t* contacts);

/// @return <0-error, other-ok
int sip_uri_write(const struct sip_uri_t* uri, char* data, const char* end);
int sip_via_write(const struct sip_via_t* via, char* data, const char* end);
int sip_cseq_write(const struct sip_cseq_t* cseq, char* data, const char* end);
int sip_contact_write(const struct sip_contact_t* contact, char* data, const char* end);

const struct cstring_t* sip_via_branch(const struct sip_via_t* via);
const struct cstring_t* sip_vias_top_branch(const struct sip_vias_t* vias);

#endif /* !_sip_header_h_ */
