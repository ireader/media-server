// RFC3216 20.42 Via (p179)
// Via: SIP/2.0/UDP erlang.bell-telephone.com:5060;branch=z9hG4bK87asdks7
// Via: SIP/2.0/UDP 192.0.2.1:5060;received=192.0.2.207;branch=z9hG4bK77asjd
// Via: SIP/2.0/UDP first.example.com:4000;ttl=16;maddr=224.2.0.1;branch=z9hG4bKa7c6a8dlze.1

// (p232)
/*
Via = ( "Via" / "v" ) HCOLON via-parm *(COMMA via-parm)
via-parm = sent-protocol LWS sent-by *( SEMI via-params )
via-params = via-ttl / via-maddr / via-received / via-branch / via-extension
via-ttl = "ttl" EQUAL ttl
via-maddr = "maddr" EQUAL host
via-received = "received" EQUAL (IPv4address / IPv6address)
via-branch = "branch" EQUAL token
via-extension = generic-param
sent-protocol = protocol-name SLASH protocol-version SLASH transport
protocol-name = "SIP" / token
protocol-version = token
transport = "UDP" / "TCP" / "TLS" / "SCTP" / other-transport
sent-by = host [ COLON port ]
ttl = 1*3DIGIT ; 0 to 255
*/

#include "sip-header.h"

void sip_via_params_free(struct sip_via_t* via)
{
	sip_params_free(&via->params);
}

int sip_header_via(const char* s, const char* end, struct sip_via_t* via)
{
	int i, j, k, r;
	const char* p;
	const struct sip_param_t* param;
	memset(via, 0, sizeof(*via));
	via->rport = -1;
	sip_params_init(&via->params);

	// SIP/2.0/UDP erlang.bell-telephone.com:5060;branch=z9hG4bK87asdks7
	sscanf(s, " %n%*[^/ \t]%n / %n%*[^/ \t]%n / %n%*[^/ \t]%n %n%*[^; \t\r\n]%n ", &i, (int*)&via->protocol.n, &j, (int*)&via->version.n, &k, (int*)&via->transport.n, &r, (int*)&via->host.n);
	if (0 == via->host.n || s + via->host.n > end)
		return EINVAL;

	// protocol-name
	via->protocol.p = s + i;
	via->protocol.n -= i;

	// protocol-version
	via->version.p = s + j;
	via->version.n -= j;
	
	// transport
	via->transport.p = s + k;
	via->transport.n -= k;

	// sent-by
	via->host.p = s + r;
	via->host.n -= r;

	// via-params
	p = strchr(via->host.p + via->host.n, ';');
	if (!p || p > end)
		p = end;

	r = 0;
	if (p && p < end && ';' == *p)
	{
		r = sip_header_params(';', p + 1, end, &via->params);

		for (i = 0; i < sip_params_count(&via->params); i++)
		{
			param = sip_params_get(&via->params, i);

			if (0 == cstrcmp(&param->name, "branch"))
			{
				via->branch.p = param->value.p;
				via->branch.n = param->value.n;
			}
			else if (0 == cstrcmp(&param->name, "maddr"))
			{
				via->maddr.p = param->value.p;
				via->maddr.n = param->value.n;
			}
			else if (0 == cstrcmp(&param->name, "received"))
			{
				via->received.p = param->value.p;
				via->received.n = param->value.n;
			}
			else if (0 == cstrcmp(&param->name, "ttl"))
			{
				via->ttl = atoi(param->value.p);
			}
			else if (0 == cstrcmp(&param->name, "rport"))
			{
				via->rport = cstrvalid(&param->value) ? (int)cstrtol(&param->value, NULL, 10) : -1;
			}
		}
	}

	return r;
}

int sip_header_vias(const char* s, const char* end, struct sip_vias_t* vias)
{
	int r;
	const char* p;
	struct sip_via_t v;

	for (r = 0; 0 == r && s && s < end; s = p + 1)
	{
		// filter ","
		p = strpbrk(s, ",\"");
		while (p && p < end && '"' == *p)
		{
			p = strchr(p + 1, '"');
			if (p && p < end)
				p = strpbrk(p + 1, ",\"");
		}
		if (!p || p >= end)
			p = end;

		//memset(&v, 0, sizeof(v));
		//sip_params_init(&v.params);
		r = sip_header_via(s, p, &v);
		if (0 == r)
			r = sip_vias_push(vias, &v);
	}

	return r;
}

// SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bK776asdhds
int sip_via_write(const struct sip_via_t* via, char* data, const char* end)
{
	int n;
	char* p;
	if (!cstrvalid(&via->protocol) || !cstrvalid(&via->version) || !cstrvalid(&via->transport) || !cstrvalid(&via->host))
		return -1;

	p = data;
	if (p < end)
		p += snprintf(p, end - p, "%.*s/%.*s/%.*s %.*s", (int)via->protocol.n, via->protocol.p, (int)via->version.n, via->version.p, (int)via->transport.n, via->transport.p, (int)via->host.n, via->host.p);

	if (sip_params_count(&via->params) > 0)
	{
		if (p < end) *p++ = ';';
		n = sip_params_write(&via->params, p, end, ';');
		if (n < 0) return n;
		p += n;
	}

	if (p < end) *p = '\0';
	return (int)(p - data);
}

const struct cstring_t* sip_vias_top_branch(const struct sip_vias_t* vias)
{
	const struct sip_via_t *via;
	via = sip_vias_get(vias, 0);
	return via ? &via->branch : NULL;
}

//const struct cstring_t* sip_via_branch(const struct sip_via_t* via)
//{
//	return sip_params_find_string(&via->params, "branch");
//}

#if defined(DEBUG) || defined(_DEBUG)
void sip_header_via_test(void)
{
	char p[1024];
	const char* s;
	const struct sip_via_t* v;
	struct sip_via_t via;
	struct sip_vias_t vias;

	s = "SIP/2.0/UDP erlang.bell-telephone.com:5060;branch=z9hG4bK87asdks7";
	assert(0 == sip_header_via(s, s + strlen(s), &via)); v = &via;
	assert(0 == cstrcmp(&v->protocol, "SIP") && 0 == cstrcmp(&v->version, "2.0") && 0 == cstrcmp(&v->transport, "UDP") && 0 == cstrcmp(&v->host, "erlang.bell-telephone.com:5060"));
	assert(1 == sip_params_count(&v->params) && 0 == cstrcmp(&sip_params_get(&v->params, 0)->name, "branch") && 0 == cstrcmp(&sip_params_get(&v->params, 0)->value, "z9hG4bK87asdks7"));
	assert(0 == cstrcmp(&v->branch, "z9hG4bK87asdks7"));
	assert(sip_via_write(v, p, p + sizeof(p)) < sizeof(p) && 0 == strcmp(s, p));
	sip_via_params_free(&via);

	sip_vias_init(&vias);
	s = "SIP/2.0/UDP first.example.com:4000;ttl=16;maddr=224.2.0.1;branch=z9hG4bKa7c6a8dlze.1,SIP/2.0/UDP erlang.bell-telephone.com:5060;branch=z9hG4bK87asdks7";
	assert(0 == sip_header_vias(s, s + strlen(s), &vias) && 2 == sip_vias_count(&vias));
	v = sip_vias_get(&vias, 0);
	assert(0 == cstrcmp(&v->protocol, "SIP") && 0 == cstrcmp(&v->version, "2.0") && 0 == cstrcmp(&v->transport, "UDP") && 0 == cstrcmp(&v->host, "first.example.com:4000"));
	assert(3 == sip_params_count(&v->params) && 0 == cstrcmp(&sip_params_get(&v->params, 2)->name, "branch") && 0 == cstrcmp(&sip_params_get(&v->params, 2)->value, "z9hG4bKa7c6a8dlze.1"));
	assert(0 == cstrcmp(&sip_params_get(&v->params, 0)->name, "ttl") && 0 == cstrcmp(&sip_params_get(&v->params, 0)->value, "16"));
	assert(0 == cstrcmp(&sip_params_get(&v->params, 1)->name, "maddr") && 0 == cstrcmp(&sip_params_get(&v->params, 1)->value, "224.2.0.1"));
	assert(v->ttl == 16 && 0 == cstrcmp(&v->maddr, "224.2.0.1") && 0 == cstrcmp(&v->branch, "z9hG4bKa7c6a8dlze.1"));
	v = sip_vias_get(&vias, 1);
	assert(0 == cstrcmp(&v->protocol, "SIP") && 0 == cstrcmp(&v->version, "2.0") && 0 == cstrcmp(&v->transport, "UDP") && 0 == cstrcmp(&v->host, "erlang.bell-telephone.com:5060"));
	assert(1 == sip_params_count(&v->params) && 0 == cstrcmp(&sip_params_get(&v->params, 0)->name, "branch") && 0 == cstrcmp(&sip_params_get(&v->params, 0)->value, "z9hG4bK87asdks7"));
	assert(0 == cstrcmp(&v->branch, "z9hG4bK87asdks7"));
	sip_vias_free(&vias);
}
#endif
