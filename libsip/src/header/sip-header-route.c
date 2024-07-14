// RFC3261 20.30 Record-Route / 20.43 Route
// Record-Route: <sip:p1.example.com;lr>
// Route: <sip:p1.example.com;lr>,<sip:p2.domain.com;lr>

/*
Record-Route  =  "Record-Route" HCOLON rec-route *(COMMA rec-route)
rec-route     =  name-addr *( SEMI rr-param )
rr-param      =  generic-param
Route        =  "Route" HCOLON route-param *(COMMA route-param)
route-param  =  name-addr *( SEMI rr-param )
name-addr = [ display-name ] LAQUOT addr-spec RAQUOT
addr-spec = SIP-URI / SIPS-URI / absoluteURI
*/

#include "sip-header.h"
#include <stdlib.h>
#include <ctype.h>

int sip_header_route(const char* s, const char* end, struct sip_uri_t* uri)
{
	const char* p;

	p = (s && s < end) ? strpbrk(s, "<\"") : NULL;
	if (!p || p > end)
	{
		// addr-spec only
		return sip_header_uri(s, end, uri);
	}

	if ('"' == *p)
	{
		// name-addr: [ display-name ]
		p = strchr(p + 1, '"');
		if (!p || p > end)
			return -EINVAL;

		// LAQUOT addr-spec RAQUOT
		p = strchr(p + 1, '<');
		if (!p || p > end)
			return -EINVAL;
	}

	if ('<' == *p)
	{
		s = p + 1;
		p = s < end ? strchr(s, '>') : NULL;
		if (!p || p > end)
			return -EINVAL;
	}

	return sip_header_uri(s, p, uri);
}

int sip_header_routes(const char* s, const char* end, struct sip_uris_t* routes)
{
	int r;
	const char* p;
	struct sip_uri_t c;

	for (r = 0; 0 == r && s && s < end; s = p + 1)
	{
		// filter ","
		p = strpbrk(s, ",\"");
		while (p && p + 1 < end && '"' == *p)
		{
			p = strchr(p + 1, '"');
			if (p && p < end)
				p = strpbrk(p + 1, ",\"");
		}
		if (!p || p >= end)
			p = end;

		r = sip_header_route(s, p, &c);
		if (0 == r)
			r = sip_uris_push(routes, &c);
	}

	return r;
}

// Route: <sip:p1.example.com;lr>,<sip:p2.domain.com;lr>
int sip_route_write(const struct sip_uri_t* route, char* data, const char* end)
{
	int n;
	char* p;

	p = data;
	if (p < end) *p++ = '<';
	n = sip_uri_write(route, p, end);
	if (n < 0) return n;
	p += n;
	if (p < end) *p++ = '>';

	if (p < end) *p = '\0';
	return (int)(p - data);
}

#if defined(DEBUG) || defined(_DEBUG)
void sip_header_route_test(void)
{
	char p[1024];
	const char* s;
	const struct sip_uri_t* c;
	struct sip_uri_t route;
	struct sip_uris_t routes;

	sip_uris_init(&routes);
	s = "\"Mr.Watson\" <sip:watson@worcester.bell-telephone.com>;q=0.7; expires=3600,\"Mr.Watson\" <mailto:watson@bell-telephone.com> ;q=0.1";
	assert(0 == sip_header_routes(s, s + strlen(s), &routes) && 2 == sip_uris_count(&routes));
	c = sip_uris_get(&routes, 0);
	assert(0 == cstrcmp(&c->scheme, "sip") && 0 == cstrcmp(&c->host, "watson@worcester.bell-telephone.com") && 0 == sip_params_count(&c->headers) && 0 == sip_params_count(&c->parameters));
	c = sip_uris_get(&routes, 1);
	assert(0 == cstrcmp(&c->scheme, "mailto") && 0 == cstrcmp(&c->host, "watson@bell-telephone.com") && 0 == sip_params_count(&c->headers) && 0 == sip_params_count(&c->parameters));
	sip_uris_free(&routes);

	s = "<sips:bob@192.0.2.4>;expires=60";
	assert(0 == sip_header_route(s, s + strlen(s), &route));
	assert(0 == cstrcmp(&route.scheme, "sips") && 0 == cstrcmp(&route.host, "bob@192.0.2.4") && 0 == sip_params_count(&route.headers) && 0 == sip_params_count(&route.parameters));
	assert(sip_route_write(&route, p, p + sizeof(p)) < sizeof(p) && 0 == strncmp(s, p, 20));
	sip_uri_free(&route);

	s = "\"<sip:joe@big.org>\" <sip:joe@really.big.com>";
	assert(0 == sip_header_route(s, s + strlen(s), &route));
	assert(0 == cstrcmp(&route.scheme, "sip") && 0 == cstrcmp(&route.host, "joe@really.big.com") && 0 == sip_params_count(&route.headers) && 0 == sip_params_count(&route.parameters));
	assert(sip_route_write(&route, p, p + sizeof(p)) < sizeof(p) && 0 == strcmp(s+20, p));
	sip_uri_free(&route);

	s = "<sip:p1.example.com;lr>,<sip:p2.domain.com;lr>";
	sip_uris_init(&routes);
	assert(0 == sip_header_routes(s, s + strlen(s), &routes) && 2 == sip_uris_count(&routes));
	c = sip_uris_get(&routes, 0);
	assert(0 == cstrcmp(&c->scheme, "sip") && 0 == cstrcmp(&c->host, "p1.example.com") && 0 == sip_params_count(&c->headers) && 1 == sip_params_count(&c->parameters));
	assert(0 == cstrcmp(&sip_params_get(&c->parameters, 0)->name, "lr") && c->lr);
	c = sip_uris_get(&routes, 1);
	assert(0 == cstrcmp(&c->scheme, "sip") && 0 == cstrcmp(&c->host, "p2.domain.com") && 0 == sip_params_count(&c->headers) && 1 == sip_params_count(&c->parameters));
	assert(0 == cstrcmp(&sip_params_get(&c->parameters, 0)->name, "lr") && c->lr);
	//assert(sip_route_write(c, p, p + sizeof(p)) < sizeof(p) && 0 == strcmp(s, p));
	sip_uris_free(&routes);
}
#endif
