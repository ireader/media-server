#include "sip-header.h"
#include <stdlib.h>
#include <string.h>

DARRAY_IMPLEMENT(sip_uri);
DARRAY_IMPLEMENT(sip_param);
DARRAY_IMPLEMENT(sip_contact);
DARRAY_IMPLEMENT(sip_via);

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

int sip_header_param_find(const struct sip_param_t* params, int n, const char* name)
{
	while(--n > 0)
	{
		if (0 == cstrcmp(&params[n].name, name))
			return n;
	}
	return -1;
}

int sip_header_param_int(const struct sip_param_t* params, int n, const char* name, int* value)
{
	n = sip_header_param_find(params, n, name);
	if (-1 == n || NULL == params[n].value.p)
		return -1;
	*value = atoi(params[n].value.p);
	return 0;
}

int sip_header_param_int64(const struct sip_param_t* params, int n, const char* name, int64_t* value)
{
	n = sip_header_param_find(params, n, name);
	if (-1 == n || NULL == params[n].value.p)
		return -1;
	*value = atoll(params[n].value.p);
	return 0;
}

int sip_header_param_double(const struct sip_param_t* params, int n, const char* name, double* value)
{
	n = sip_header_param_find(params, n, name);
	if (-1 == n || NULL == params[n].value.p)
		return -1;
	*value = atof(params[n].value.p);
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

int sip_params_write(const struct sip_params_t* params, char* data, const char* end)
{
	char* p;
	int i, n;

	p = data;
	for (i = 0; i < sip_params_count((struct sip_params_t*)params) && p < end; i++)
	{
		*p++ = ';';
		n = sip_param_write(sip_params_get((struct sip_params_t*)params, i), p, end);
		if (n < 0) return n;
		p += n;
	}

	return p - data;
}

#if defined(DEBUG) || defined(_DEBUG)
void sip_header_param_test(void)
{
	const char* s;
	struct sip_param_t param;
	
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
