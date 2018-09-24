#include "sip-header.h"
#include <stdlib.h>
#include <string.h>

static void sip_param_params_free(struct sip_param_t* param)
{
}

DARRAY_IMPLEMENT(sip_uri, 3);
DARRAY_IMPLEMENT(sip_param, 5);
DARRAY_IMPLEMENT(sip_contact, 3);
DARRAY_IMPLEMENT(sip_via, 8);

int sip_header_param(const char* s, const char* end, struct sip_param_t* param)
{
	const char* p;
	param->name.p = s;

	p = strchr(s, '=');
	if (p && p < end)
	{
		param->name.n = p - s;
		param->value.p = p + 1;
		param->value.n = end - param->value.p;

		cstrtrim(&param->value, " \t");
	}
	else
	{
		param->name.n = end - s;
		param->value.p = NULL;
		param->value.n = 0;
	}

	cstrtrim(&param->name, " \t");
	return 0;
}

int sip_header_params(char sep, const char* s, const char* end, struct sip_params_t* params)
{
	int r;
	const char* p;
	struct sip_param_t param;

	r = 0;
	for (p = s; 0 == r && p && p < end; p++)
	{
		s = p;
		p = strchr(s, sep);
		if (!p || p > end)
			p = end;

		memset(&param, 0, sizeof(param));
		r = sip_header_param(s, p, &param);
		r = sip_params_push(params, &param);
	}

	return r;
}

const struct sip_param_t* sip_params_find(const struct sip_params_t* params, const char* name, int bytes)
{
	int i;
	struct cstring_t s;
	const struct sip_param_t* p;
	s.p = name;
	s.n = bytes;
	for(i = 0; i < sip_params_count(params); i++)
	{
		p = sip_params_get(params, i);
		if (cstreq(&p->name, &s))
			return p;
	}
	return NULL;
}

const struct cstring_t* sip_params_find_string(const struct sip_params_t* params, const char* name, int bytes)
{
	const struct sip_param_t* p;
	p = sip_params_find(params, name, bytes);
	return p ? &p->value : NULL;
}

int sip_params_find_int(const struct sip_params_t* params, const char* name, int bytes, int* value)
{
	const struct sip_param_t* p;
	p = sip_params_find(params, name, bytes);
	if (NULL == p) return -ENOENT; // not found
	*value = cstrtol(&p->value, NULL, 10);
	return 0;
}

int sip_params_find_int64(const struct sip_params_t* params, const char* name, int bytes, int64_t* value)
{
	const struct sip_param_t* p;
	p = sip_params_find(params, name, bytes);
	if (NULL == p) return -ENOENT; // not found
	*value = cstrtoll(&p->value, NULL, 10);
	return 0;
}

int sip_params_find_double(const struct sip_params_t* params, const char* name, int bytes, double* value)
{
	const struct sip_param_t* p;
	p = sip_params_find(params, name, bytes);
	if (NULL == p) return -ENOENT; // not found
	*value = cstrtod(&p->value, NULL);
	return 0;
}

int sip_param_write(const struct sip_param_t* param, char* data, const char* end)
{
	char* p;
	if (!cstrvalid(&param->name))
		return -1;

	p = data;
	if (p < end) p += cstrcpy(&param->name, p, end - p);
	
	if (cstrvalid(&param->value))
	{
		if (p < end) *p++ = '=';
		if (p < end) p += cstrcpy(&param->value, p, end - p);
	}

	return p - data;
}

int sip_params_write(const struct sip_params_t* params, char* data, const char* end, char sep)
{
	char* p;
	int i, n;
	const struct sip_param_t* param;

	p = data;
	for (i = 0; i < sip_params_count(params) && p < end; i++)
	{
		param = sip_params_get(params, i);

		if(i > 0) *p++ = sep;
		n = sip_param_write(param, p, end);
		if (n < 0) return n;
		p += n;
	}

	return p - data;
}

#if defined(DEBUG) || defined(_DEBUG)
void sip_header_param_test(void)
{
	const char* s;
	struct cstring_t x;
	struct sip_param_t param;
	
	x.p = "0x12345678";
	x.n = 8;
	assert(0x123456 == cstrtol(&x, NULL, 16));
	x.n = 10;
	assert(0x12345678 == cstrtol(&x, NULL, 16));

	s = "name=value";
	assert(0 == sip_header_param(s, s + strlen(s), &param));
	assert(4 == param.name.n && 0 == cstrcmp(&param.name, "name"));
	assert(5 == param.value.n && 0 == cstrcmp(&param.value, "value"));

	s = "name=";
	assert(0 == sip_header_param(s, s + strlen(s), &param));
	assert(4 == param.name.n && 0 == cstrcmp(&param.name, "name"));
	assert(0 == param.value.n && 0 == cstrcmp(&param.value, ""));

	s = "=value";
	assert(0 == sip_header_param(s, s + strlen(s), &param));
	assert(0 == param.name.n && 0 == cstrcmp(&param.name, ""));
	assert(5 == param.value.n && 0 == cstrcmp(&param.value, "value"));

	s = "=";
	assert(0 == sip_header_param(s, s + strlen(s), &param));
	assert(0 == param.name.n && 0 == cstrcmp(&param.name, ""));
	assert(0 == param.value.n && 0 == cstrcmp(&param.value, ""));

	s = "name";
	assert(0 == sip_header_param(s, s + strlen(s), &param));
	assert(4 == param.name.n && 0 == cstrcmp(&param.name, "name"));
	assert(0 == param.value.n && NULL == param.value.p);
}
#endif
