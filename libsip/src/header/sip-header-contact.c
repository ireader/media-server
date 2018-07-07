// RFC3261 20.10 Contact (p167)
// Contact: "Mr. Watson" <sip:watson@worcester.bell-telephone.com>;q=0.7; expires=3600,"Mr. Watson" <mailto:watson@bell-telephone.com> ;q=0.1
// The compact form of the Contact header field is m (for "moved").
// m: <sips:bob@192.0.2.4>;expires=60

// 25.1 Basic Rules
// HCOLON = *( SP / HTAB ) ":" SWS
// LWS = [*WSP CRLF] 1*WSP ; linear whitespace
// SWS = [LWS] ; sep whitespace
// STAR = SWS "*" SWS ; asterisk
// SLASH = SWS "/" SWS ; slash
// EQUAL = SWS "=" SWS ; equal
// LPAREN = SWS "(" SWS ; left parenthesis
// RPAREN = SWS ")" SWS ; right parenthesis
// RAQUOT = ">" SWS ; right angle quote
// LAQUOT = SWS "<"; left angle quote
// COMMA = SWS "," SWS ; comma
// SEMI = SWS ";" SWS ; semicolon
// COLON = SWS ":" SWS ; colon
// LDQUOT = SWS DQUOTE; open double quotation mark
// RDQUOT = DQUOTE SWS ; close double quotation mark
// quoted-string = SWS DQUOTE *(qdtext / quoted-pair ) DQUOTE
// qdtext = LWS / %x21 / %x23-5B / %x5D-7E / UTF8-NONASCII
// quoted-pair = "\" (%x00-09 / %x0B-0C / %x0E-7F)

/*
SIP-URI = "sip:" [ userinfo ] hostport uri-parameters [ headers ]
SIPS-URI = "sips:" [ userinfo ] hostport uri-parameters [ headers ]
userinfo = ( user / telephone-subscriber ) [ ":" password ] "@"
user = 1*( unreserved / escaped / user-unreserved )
user-unreserved = "&" / "=" / "+" / "$" / "," / ";" / "?" / "/"
password = *( unreserved / escaped / "&" / "=" / "+" / "$" / "," )
hostport = host [ ":" port ]
host = hostname / IPv4address / IPv6reference
hostname = *( domainlabel "." ) toplabel [ "." ]
domainlabel = alphanum / alphanum *( alphanum / "-" ) alphanum
toplabel = ALPHA / ALPHA *( alphanum / "-" ) alphanum

Contact = ("Contact" / "m" ) HCOLON ( STAR / (contact-param *(COMMA contact-param)))
contact-param = (name-addr / addr-spec) *(SEMI contact-params)
name-addr = [ display-name ] LAQUOT addr-spec RAQUOT
addr-spec = SIP-URI / SIPS-URI / absoluteURI
display-name = *(token LWS)/ quoted-string
contact-params = c-p-q / c-p-expires / contact-extension
c-p-q = "q" EQUAL qvalue
c-p-expires = "expires" EQUAL delta-seconds
contact-extension = generic-param
delta-seconds = 1*DIGIT
*/

#include "sip-header.h"
#include <stdlib.h>
#include <ctype.h>

static int sip_header_contact_item(const char* s, const char* end, struct sip_contact_t* c)
{
	int r;
	const char* p;
	struct sip_param_t param;

	p = strpbrk(s, "<;\"");
	if (!p || p > end)
	{
		// addr-spec only
		return sip_header_uri(s, end, &c->uri);
	}

	if ('"' == *p)
	{
		// name-addr: [ display-name ]
		p = strchr(p + 1, '"');
		if (!p || p > end)
			return EINVAL;

		// LAQUOT addr-spec RAQUOT
		p = strchr(p + 1, '<');
		if (!p || p > end)
			return EINVAL;
	}

	if ('<' == *p)
	{
		// [ display-name ]
		c->nickname.p = s;
		c->nickname.n = p - s;
		cstrtrim(&c->nickname, " \t");
		cstrtrim(&c->nickname, "\""); // "nickname" => nickname

		s = p + 1;
		p = strchr(s, '>');
		if (!p || p > end)
			return EINVAL;

		r = sip_header_uri(s, p, &c->uri);
		if (0 != r)
			return r;

		s = p + 1;
		p = strchr(s, ';');
	}
	else
	{
		r = sip_header_uri(s, p, &c->uri);
		if (0 != r)
			return r;
	}

	while (0 == r && p && p < end && ';' == *p)
	{
		s = p + 1;
		p = strchr(s, ';');
		if (!p || p >= end)
			p = end;

		sip_header_param(s, p, &param);
		r = sip_params_push(&c->params, &param);

		//if (0 == cstrcmp(&param.name, "tag"))
		//{
		//	c->tag.p = param.value.p;
		//	c->tag.n = param.value.n;
		//}
	}

	return 0;
}

int sip_header_contact(const char* s, const char* end, struct sip_contacts_t* contacts)
{
	int r;
	const char* p;
	struct sip_contact_t c, *pc;

	for (r = 0; 0 == r && s && s < end; s = p + 1)
	{
		p = strchr(s, ',');
		if (!p) p = end;

		memset(&c, 0, sizeof(c));
		sip_params_init(&c.params);
		r = sip_header_contact_item(s, p, &c);
		if (0 == r)
		{
			r = sip_contacts_push(contacts, &c);
			pc = sip_contacts_get(contacts, sip_contacts_count(contacts) - 1);
			if (pc->params.arr.elements == c.params.ptr)
				pc->params.arr.elements = pc->params.ptr;
		}
	}

	return r;
}

static int sip_nickname_check(const struct cstring_t* s)
{
	size_t i;
	for (i = 0; i < s->n; i++)
	{
		if (!isalnum(s->p[i]))
			return 0;
	}
	return 1;
}

// Alice <sip:alice@atlanta.com>;tag=1928301774
int sip_contact_write(const struct sip_contact_t* c, char* data, const char* end)
{
	int n;
	char* p;

	p = data;
	if (c->nickname.p && c->nickname.n > 0)
	{
		if (sip_nickname_check(&c->nickname))
		{
			if (p < end) p += cstrcpy(&c->nickname, p, end - p);
		}
		else
		{
			if (p < end) *p++ = '\"';
			if (p < end) p += cstrcpy(&c->nickname, p, end - p - 1);
			if (p < end) *p++ = '\"';
		}

		if (p < end) *p++ = ' ';
	}

	if (p < end) *p++ = '<';
	n = sip_uri_write(&c->uri, p, end);
	if (n < 0) return n;
	if (p < end) *p++ = '>';

	n = sip_params_write(&c->params, p, end);
	if (n < 0) return n;
	p += n;
	
	return p - data;
}

#if defined(DEBUG) || defined(_DEBUG)
void sip_header_contact_test(void)
{
	const char* s;
	struct sip_contact_t* c;
	struct sip_contacts_t contacts;

	sip_contacts_init(&contacts);
	s = "\"Mr.Watson\" <sip:watson@worcester.bell-telephone.com>;q=0.7; expires=3600,\"Mr.Watson\" <mailto:watson@bell-telephone.com> ;q=0.1";
	assert(0 == sip_header_contact(s, s + strlen(s), &contacts) && 2 == sip_contacts_count(&contacts));
	c = sip_contacts_get(&contacts, 0);
	assert(0 == cstrcmp(&c->nickname, "Mr.Watson") && 0 == cstrcmp(&c->uri.scheme, "sip") && 0 == cstrcmp(&c->uri.host, "watson@worcester.bell-telephone.com") && 0 == c->uri.headers.n && 0 == c->uri.parameters.n);
	assert(2 == sip_params_count(&c->params));
	assert(0 == cstrcmp(&sip_params_get(&c->params, 0)->name, "q") && 0 == cstrcmp(&sip_params_get(&c->params, 0)->value, "0.7"));
	assert(0 == cstrcmp(&sip_params_get(&c->params, 1)->name, "expires") && 0 == cstrcmp(&sip_params_get(&c->params, 1)->value, "3600"));
	c = sip_contacts_get(&contacts, 1);
	assert(0 == cstrcmp(&c->nickname, "Mr.Watson") && 0 == cstrcmp(&c->uri.scheme, "mailto") && 0 == cstrcmp(&c->uri.host, "watson@bell-telephone.com") && 0 == c->uri.headers.n && 0 == c->uri.parameters.n);
	assert(1 == sip_params_count(&c->params) && 0 == cstrcmp(&sip_params_get(&c->params, 0)->name, "q") && 0 == cstrcmp(&sip_params_get(&c->params, 0)->value, "0.1"));
	sip_contacts_free(&contacts);

	sip_contacts_init(&contacts);
	s = "<sips:bob@192.0.2.4>;expires=60";
	assert(0 == sip_header_contact(s, s + strlen(s), &contacts) && 1 == sip_contacts_count(&contacts));
	c = sip_contacts_get(&contacts, 0);
	assert(0 == c->nickname.n && 0 == cstrcmp(&c->uri.scheme, "sips") && 0 == cstrcmp(&c->uri.host, "bob@192.0.2.4") && 0 == c->uri.headers.n && 0 == c->uri.parameters.n);
	assert(1 == sip_params_count(&c->params) && 0 == cstrcmp(&sip_params_get(&c->params, 0)->name, "expires") && 0 == cstrcmp(&sip_params_get(&c->params, 0)->value, "60"));
	sip_contacts_free(&contacts);

	sip_contacts_init(&contacts);
	s = "\"<sip:joe@big.org>\" <sip:joe@really.big.com>";
	assert(0 == sip_header_contact(s, s + strlen(s), &contacts) && 1 == sip_contacts_count(&contacts));
	c = sip_contacts_get(&contacts, 0);
	assert(0 == cstrcmp(&c->nickname, "<sip:joe@big.org>") && 0 == cstrcmp(&c->uri.scheme, "sip") && 0 == cstrcmp(&c->uri.host, "joe@really.big.com") && 0 == c->uri.headers.n && 0 == c->uri.parameters.n);
	sip_contacts_free(&contacts);
	
	sip_contacts_init(&contacts);
	s = "*";
	sip_contacts_free(&contacts);

	// TO/FROM
	sip_contacts_init(&contacts);
	s = "Alice <sip:alice@atlanta.com>;tag=1928301774";
	assert(0 == sip_header_contact(s, s + strlen(s), &contacts) && 1 == sip_contacts_count(&contacts));
	c = sip_contacts_get(&contacts, 0);
	assert(0 == cstrcmp(&c->nickname, "Alice") && 0 == cstrcmp(&c->uri.scheme, "sip") && 0 == cstrcmp(&c->uri.host, "alice@atlanta.com") && 0 == c->uri.headers.n && 0 == c->uri.parameters.n);
	assert(1 == sip_params_count(&c->params) && 0 == cstrcmp(&sip_params_get(&c->params, 0)->name, "tag") && 0 == cstrcmp(&sip_params_get(&c->params, 0)->value, "1928301774"));
	//assert(0 == cstrcmp(&c->tag, "1928301774"));
	sip_contacts_free(&contacts);
}
#endif
