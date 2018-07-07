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

static int sip_header_via_item(const char* s, const char* end, struct sip_via_t* via)
{
	int r;
	const char* p;
	struct sip_param_t param;

	p = strchr(s, '/');
	if (!p || p > end)
		return EINVAL;

	// protocol-name
	via->protocol.p = s;
	via->protocol.n = p - s;
	cstrtrim(&via->protocol, " \t");

	s = p + 1;
	p = strchr(s, '/');
	if (!p || p > end)
		return EINVAL;

	// protocol-version
	via->version.p = s;
	via->version.n = p - s;
	cstrtrim(&via->version, " \t");

	s = p + 1;
	p = strpbrk(s, " \t");
	if (!p || p > end)
		return EINVAL;

	// transport
	via->transport.p = s;
	via->transport.n = p - s;
	cstrtrim(&via->transport, " \t");

	s = p + 1;
	p = strchr(s, ';');
	if (!p || p > end)
		p = end;

	// sent-by
	via->host.p = s;
	via->host.n = p - s;
	cstrtrim(&via->host, " \t");

	// via-params
	r = 0;
	while (0 == r && p && p < end && ';' == *p)
	{
		s = p + 1;
		p = strchr(s, ';');
		if (!p || p >= end)
			p = end;

		sip_header_param(s, p, &param);
		r = sip_params_push(&via->params, &param);
	}

	return 0;
}

int sip_header_via(const char* s, const char* end, struct sip_vias_t* vias)
{
	int r;
	const char* p;
	struct sip_via_t v, *pv;

	for (r = 0; 0 == r && s && s < end; s = p + 1)
	{
		p = strchr(s, ',');
		if (!p) p = end;

		memset(&v, 0, sizeof(v));
		sip_params_init(&v.params);
		r = sip_header_via_item(s, p, &v);
		if (0 == r)
		{
			r = sip_vias_push(vias, &v);
			pv = sip_vias_get(vias, sip_vias_count(vias) - 1);
			if (pv->params.arr.elements == v.params.ptr)
				pv->params.arr.elements = pv->params.ptr;
		}
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
	if (p < end) p += cstrcpy(&via->protocol, p, end - p);
	if (p < end) *p++ = '/';
	if (p < end) p += cstrcpy(&via->version, p, end - p);
	if (p < end) *p++ = '/';
	if (p < end) p += cstrcpy(&via->transport, p, end - p);
	if (p < end) *p++ = ' ';
	if (p < end) p += cstrcpy(&via->host, p, end - p);

	n = sip_params_write(&via->params, p, end);
	if (n < 0) return n;
	p += n;

	return p - data;
}

const struct cstring_t* sip_vias_top_branch(const struct sip_vias_t* vias)
{
	const struct sip_via_t *via;
	via = sip_vias_get(vias, 0);
	return via ? sip_via_branch(via) : NULL;
}

const struct cstring_t* sip_via_branch(const struct sip_via_t* via)
{
	return sip_params_find_string(&via->params, "branch");
}

#if defined(DEBUG) || defined(_DEBUG)
void sip_header_via_test(void)
{
	const char* s;
	struct sip_via_t* v;
	struct sip_vias_t vias;

	sip_vias_init(&vias);
	s = "SIP/2.0/UDP erlang.bell-telephone.com:5060;branch=z9hG4bK87asdks7";
	assert(0 == sip_header_via(s, s + strlen(s), &vias) && 1 == sip_vias_count(&vias));
	v = sip_vias_get(&vias, 0);
	assert(0 == cstrcmp(&v->protocol, "SIP") && 0 == cstrcmp(&v->version, "2.0") && 0 == cstrcmp(&v->transport, "UDP") && 0 == cstrcmp(&v->host, "erlang.bell-telephone.com:5060"));
	assert(1 == sip_params_count(&v->params) && 0 == cstrcmp(&sip_params_get(&v->params, 0)->name, "branch") && 0 == cstrcmp(&sip_params_get(&v->params, 0)->value, "z9hG4bK87asdks7"));
	sip_vias_free(&vias);

	sip_vias_init(&vias);
	s = "SIP/2.0/UDP first.example.com:4000;ttl=16;maddr=224.2.0.1;branch=z9hG4bKa7c6a8dlze.1";
	assert(0 == sip_header_via(s, s + strlen(s), &vias) && 1 == sip_vias_count(&vias));
	v = sip_vias_get(&vias, 0);
	assert(0 == cstrcmp(&v->protocol, "SIP") && 0 == cstrcmp(&v->version, "2.0") && 0 == cstrcmp(&v->transport, "UDP") && 0 == cstrcmp(&v->host, "first.example.com:4000"));
	assert(3 == sip_params_count(&v->params) && 0 == cstrcmp(&sip_params_get(&v->params, 2)->name, "branch") && 0 == cstrcmp(&sip_params_get(&v->params, 2)->value, "z9hG4bKa7c6a8dlze.1"));
	assert(0 == cstrcmp(&sip_params_get(&v->params, 0)->name, "ttl") && 0 == cstrcmp(&sip_params_get(&v->params, 0)->value, "16"));
	assert(0 == cstrcmp(&sip_params_get(&v->params, 1)->name, "maddr") && 0 == cstrcmp(&sip_params_get(&v->params, 1)->value, "224.2.0.1"));
	sip_vias_free(&vias);
}
#endif
