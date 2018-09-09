// RFC3261 19.1.1 SIP and SIPS URI Components (p148)
// RFC3261 19.1.2 Character Escaping Requirements (p152)
// sip:user:password@host:port;uri-parameters?headers
// sip:alice@atlanta.com
// sip:alice:secretword@atlanta.com;transport=tcp
// sips:alice@atlanta.com?subject=project%20x&priority=urgent
// sip:+1-212-555-1212:1234@gateway.com;user=phone
// sips:1212@gateway.com
// sip:alice@192.0.2.4
// sip:atlanta.com;method=REGISTER?to=alice%40atlanta.com
// sip:alice;day=tuesday@atlanta.com

#include "sip-header.h"
#include "uri-parse.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#if defined(_WIN32) || defined(_WIN64) || defined(OS_WINDOWS)
#define strncasecmp	_strnicmp
#endif

int sip_header_uri(const char* s, const char* end, struct sip_uri_t* uri)
{
	int i;
	const char* p;
	const struct sip_param_t* param;
	struct cstring_t parameters;
	memset(uri, 0, sizeof(*uri));

	p = strchr(s, ':');
	if (!p || p >= end)
		return -1;
	
	uri->scheme.p = s;
	uri->scheme.n = p - s;
	uri->host.p = p + 1;

	s = p + 1;
	p = strpbrk(s, ";?");
	if (!p || p >= end)
	{
		uri->host.n = end - s;
	}
	else
	{
		uri->host.n = p - s;
		
		if (p && p < end && ';' == *p)
		{
			parameters.p = p + 1;
			p = strpbrk(p + 1, "?");
			if(p && p < end)
				parameters.n = p - parameters.p;
			else
				parameters.n = end - parameters.p;
			sip_header_params(';', parameters.p, parameters.p + parameters.n, &uri->parameters);
		}

		if (p && p < end && '?' == *p)
		{
			sip_header_params('&', p + 1, end, &uri->headers);
		}
	}

	for(i = 0; i < sip_params_count(&uri->parameters); i++)
	{
		param = sip_params_get(&uri->parameters, i);
		if (cstrcasecmp(&param->name, "transport"))
		{
			uri->transport.p = param->value.p;
			uri->transport.n = param->value.n;
		}
		else if (cstrcasecmp(&param->name, "method"))
		{
			uri->method.p = param->value.p;
			uri->method.n = param->value.n;
		}
		else if (cstrcasecmp(&param->name, "maddr"))
		{
			uri->maddr.p = param->value.p;
			uri->maddr.n = param->value.n;
		}
		else if (cstrcasecmp(&param->name, "user"))
		{
			uri->user.p = param->value.p;
			uri->user.n = param->value.n;
		}
		else if (cstrcasecmp(&param->name, "ttl"))
		{
			uri->ttl = cstrtol(&param->value, NULL, 10);
		}
		else if (cstrcasecmp(&param->name, "lr"))
		{
			uri->lr = cstrtol(&param->value, NULL, 10);
		}
	}

	return 0;
}

// sip:user:password@host:port;uri-parameters?headers
int sip_uri_write(const struct sip_uri_t* uri, char* data, const char* end)
{
	char* p;
	if (!cstrvalid(&uri->scheme) || !cstrvalid(&uri->host))
		return -1; // error

	p = data;
	if (p < end) p += cstrcpy(&uri->scheme, p, end - p);
	if (p < end) *p++ = ':';
	if (p < end) p += cstrcpy(&uri->host, p, end - p);

	if (sip_params_count(&uri->parameters) > 0)
	{
		*p++ = ';';
		p += sip_params_write(&uri->parameters, p, end, ';');
	}

	if (sip_params_count(&uri->headers) > 0)
	{
		*p++ = '?';
		p += sip_params_write(&uri->headers, p, end, '&');
	}

	return p - data;
}

// 19.1.4 URI Comparison (p153)
int sip_uri_equal(const struct sip_uri_t* l, const struct sip_uri_t* r)
{
	static const char* uriparameters[] = { "user", "ttl", "method" };

	int i;
	const char* p1;
	const char* p2;
	const struct sip_param_t* param1;
	const struct sip_param_t* param2;

	if (!cstreq(&l->scheme, &r->scheme))
		return 0;

	p1 = cstrchr(&l->host, '@');
	p2 = cstrchr(&r->host, '@');
	p1 = p1 ? p1 : l->host.p;
	p2 = p2 ? p2 : r->host.p;

	// Comparison of the userinfo of SIP and SIPS URIs is case-sensitive
	if (p1 - l->host.p != p2 - r->host.p || 0 != strncmp(l->host.p, r->host.p, l->host.p - p1))
		return 0;

	// Comparison of all other components of the URI is case-insensitive
	// A URI omitting any component with a default value will not match a 
	// URI explicitly containing that component with its default value.
	if (l->host.n != r->host.n || 0 != strncasecmp(p1, p2, l->host.p + l->host.n - p1))
		return 0;

	// Any uri-parameter appearing in both URIs must match.
	for (i = 0; i < sip_params_count(&l->parameters); i++)
	{
		param1 = sip_params_get(&l->parameters, i);
		if(0 == cstrcmp(&param1->name, "maddr"))
			continue;
		param2 = sip_params_find(&r->parameters, param1->name.p, param1->name.n);
		if (!param2 || !cstreq(&param1->value, &param2->value))
			return 0;
	}

	// A user, ttl, or method uri-parameter appearing in only one URI 
	// never matches, even if it contains the default value
	for (i = 0; i < sizeof(uriparameters) / sizeof(uriparameters[0]); i++)
	{
		param1 = sip_params_find(&l->parameters, uriparameters[i], strlen(uriparameters[i]));
		param2 = sip_params_find(&r->parameters, uriparameters[i], strlen(uriparameters[i]));
		if (param2 && !param1) // eq(param1, param2) already checked
			return 0;
	}

	// A URI that includes an maddr parameter will not match a URI
	// that contains no maddr parameter
	param1 = sip_params_find(&l->parameters, "maddr", 5);
	param2 = sip_params_find(&r->parameters, "maddr", 5);
	if ((param1 && !param2) || (!param1 && param2) || !cstreq(&param1->value, &param2->value))
		return 0;

	// URI header components are never ignored.
	if (sip_params_count(&l->headers) != sip_params_count(&r->headers))
		return 0;

	// The matching rules are defined for each header field in Section 20.
	for (i = 0; i < sip_params_count(&l->headers); i++)
	{
		param1 = sip_params_get(&l->headers, i);
		param2 = sip_params_find(&r->headers, param1->name.p, param1->name.n);
		if (!param2 || !cstreq(&param1->value, &param2->value))
			return 0;
	}

	return 1;
}

#if defined(DEBUG) || defined(_DEBUG)
void sip_uri_parse_test(void)
{
	char p[1024];
	const char* s;
	struct sip_uri_t uri;

	s = "sip:user:password@host:port;uri-parameters?headers";
	assert(0 == sip_header_uri(s, s + strlen(s), &uri));
	assert(0 == cstrcmp(&uri.scheme, "sip") && 0 == cstrcmp(&uri.host, "user:password@host:port") && 1 == sip_params_count(&uri.parameters) && 1 == sip_params_count(&uri.headers));
	assert(0 == cstrcmp(&sip_params_get(&uri.parameters, 0)->name, "uri-parameters") && 0 == cstrcmp(&sip_params_get(&uri.parameters, 0)->value, ""));
	assert(0 == cstrcmp(&sip_params_get(&uri.headers, 0)->name, "headers") && 0 == cstrcmp(&sip_params_get(&uri.headers, 0)->value, ""));
	assert(sip_uri_write(&uri, p, p + sizeof(p)) < sizeof(p) && 0 == strcmp(s, p));

	s = "sip:alice@atlanta.com";
	assert(0 == sip_header_uri(s, s + strlen(s), &uri));
	assert(0 == cstrcmp(&uri.scheme, "sip") && 0 == cstrcmp(&uri.host, "alice@atlanta.com") && 0 == sip_params_count(&uri.parameters) && 0 == sip_params_count(&uri.headers));
	assert(sip_uri_write(&uri, p, p + sizeof(p)) < sizeof(p) && 0 == strcmp(s, p));

	s = "sips:alice@atlanta.com?subject=project%20x&priority=urgent";
	assert(0 == sip_header_uri(s, s + strlen(s), &uri));
	assert(0 == cstrcmp(&uri.scheme, "sips") && 0 == cstrcmp(&uri.host, "alice@atlanta.com") && 0 == sip_params_count(&uri.parameters) && 2 == sip_params_count(&uri.headers));
	assert(0 == cstrcmp(&sip_params_get(&uri.headers, 0)->name, "subject") && 0 == cstrcmp(&sip_params_get(&uri.headers, 0)->value, "project%20x"));
	assert(0 == cstrcmp(&sip_params_get(&uri.headers, 1)->name, "priority") && 0 == cstrcmp(&sip_params_get(&uri.headers, 1)->value, "urgent"));
	assert(sip_uri_write(&uri, p, p + sizeof(p)) < sizeof(p) && 0 == strcmp(s, p));

	s = "sip:alice:secretword@atlanta.com;transport=tcp";
	assert(0 == sip_header_uri(s, s + strlen(s), &uri));
	assert(0 == cstrcmp(&uri.scheme, "sip") && 0 == cstrcmp(&uri.host, "alice:secretword@atlanta.com") && 0 == sip_params_count(&uri.parameters) && 0 == sip_params_count(&uri.headers));
	assert(0 == cstrcmp(&sip_params_get(&uri.parameters, 0)->name, "transport") && 0 == cstrcmp(&sip_params_get(&uri.parameters, 0)->value, "tcp"));
	assert(sip_uri_write(&uri, p, p + sizeof(p)) < sizeof(p) && 0 == strcmp(s, p));

	s = "sip:+1-212-555-1212:1234@gateway.com;user=phone";
	assert(0 == sip_header_uri(s, s + strlen(s), &uri));
	assert(0 == cstrcmp(&uri.scheme, "sip") && 0 == cstrcmp(&uri.host, "+1-212-555-1212:1234@gateway.com") && 0 == sip_params_count(&uri.parameters) && 0 == sip_params_count(&uri.headers));
	assert(0 == cstrcmp(&sip_params_get(&uri.parameters, 0)->name, "user") && 0 == cstrcmp(&sip_params_get(&uri.parameters, 0)->value, "phone"));
	assert(sip_uri_write(&uri, p, p + sizeof(p)) < sizeof(p) && 0 == strcmp(s, p));

	s = "sip:alice;day=tuesday@atlanta.com";
	assert(0 == sip_header_uri(s, s + strlen(s), &uri));
	assert(0 == cstrcmp(&uri.scheme, "sip") && 0 == cstrcmp(&uri.host, "alice") && 1 == sip_params_count(&uri.parameters) && 0 == sip_params_count(&uri.headers));
	assert(0 == cstrcmp(&sip_params_get(&uri.parameters, 0)->name, "day") && 0 == cstrcmp(&sip_params_get(&uri.parameters, 0)->value, "tuesday@atlanta.com"));
	assert(sip_uri_write(&uri, p, p + sizeof(p)) < sizeof(p) && 0 == strcmp(s, p));

	s = "sip:p2.domain.com;lr";
	assert(0 == sip_header_uri(s, s + strlen(s), &uri));
	assert(0 == cstrcmp(&uri.scheme, "sip") && 0 == cstrcmp(&uri.host, "p2.domain.com") && 1 == sip_params_count(&uri.parameters) && 0 == sip_params_count(&uri.headers));
	assert(0 == cstrcmp(&sip_params_get(&uri.parameters, 0)->name, "lr") && 0 == cstrcmp(&sip_params_get(&uri.parameters, 0)->value, ""));
	assert(sip_uri_write(&uri, p, p + sizeof(p)) < sizeof(p) && 0 == strcmp(s, p));
}
#endif
