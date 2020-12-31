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

static int sip_header_contact_star(struct sip_contact_t* c)
{
	sip_params_init(&c->uri.headers);
	sip_params_init(&c->uri.parameters);
	c->uri.host.p = "*";
	c->uri.host.n = 1;
	return 0;
}

void sip_contact_params_free(struct sip_contact_t* contact)
{
	sip_params_free(&contact->params);
	sip_uri_params_free(&contact->uri);
}

int sip_header_contact(const char* s, const char* end, struct sip_contact_t* c)
{
	int i, r;
	const char* p;
	const struct sip_param_t* param;
	memset(c, 0, sizeof(*c));
	sip_params_init(&c->params);

	if (s && 1 == end - s && '*' == *s)
	{
		return sip_header_contact_star(c);
	}

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

	if (0 == r && p && p < end && ';' == *p)
	{
		r = sip_header_params(';', p + 1, end, &c->params);

		for (i = 0; i < sip_params_count(&c->params); i++)
		{
			param = sip_params_get(&c->params, i);
			if (0 == cstrcmp(&param->name, "tag"))
			{
				c->tag.p = param->value.p;
				c->tag.n = param->value.n;
			}
			else if (0 == cstrcmp(&param->name, "q"))
			{
				c->q = strtod(param->value.p, NULL);
			}
			else if (0 == cstrcmp(&param->name, "expires"))
			{
				c->expires = strtoll(param->value.p, NULL, 10);
			}
		}
	}

	return r;
}

int sip_header_contacts(const char* s, const char* end, struct sip_contacts_t* contacts)
{
	int r;
	const char* p;
	struct sip_contact_t c;

	for (r = 0; 0 == r && s && s < end; s = p + 1)
	{
		// filter ","
		p = strpbrk(s, ",\"");
		while (p && p < end && '"' == *p)
		{
			p = strchr(p + 1, '"');
			if(p && p < end)
				p = strpbrk(p + 1, ",\"");
		}
		if (!p || p >= end)
			p = end;

		//memset(&c, 0, sizeof(c));
		//sip_params_init(&c.params);
		r = sip_header_contact(s, p, &c);
		if (0 == r)
			r = sip_contacts_push(contacts, &c);
	}

	return r;
}

int sip_contacts_match_any(const struct sip_contacts_t* contacts)
{
	int i;
	const struct sip_contact_t* contact;
	for (i = 0; i < sip_contacts_count(contacts); i++)
	{
		contact = sip_contacts_get(contacts, i);
		if (0 == cstrcmp(&contact->uri.host, "*"))
			return 1;
	}
	return 0;
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
	const char* quote;

	p = data;
	if (cstrvalid(&c->nickname) && p < end)
	{
		quote = sip_nickname_check(&c->nickname) ? "" : "\"";
		p += snprintf(p, end - p, "%s%.*s%s ", quote, (int)c->nickname.n, c->nickname.p, quote);
	}

	if (0 == cstrcmp(&c->uri.host, "*"))
	{
		if (p < end) *p++ = '*';
	}
	else
	{
		if (p < end) *p++ = '<';
		n = sip_uri_write(&c->uri, p, end);
		if (n < 0) return n;
		p += n;
		if (p < end) *p++ = '>';
	}

	if (sip_params_count(&c->params) > 0)
	{
		if (p < end) *p++ = ';';
		n = sip_params_write(&c->params, p, end, ';');
		if (n < 0) return n;
		p += n;
	}
	
	if (p < end) *p = '\0';
	return (int)(p - data);
}

//const struct cstring_t* sip_contact_tag(const struct sip_contact_t* contact)
//{
//	return sip_params_find_string(&contact->params, "tag");
//}

#if defined(DEBUG) || defined(_DEBUG)
void sip_header_contact_test(void)
{
	char p[1024];
	const char* s;
	const struct sip_contact_t* c;
	struct sip_contact_t contact;
	struct sip_contacts_t contacts;

	sip_contacts_init(&contacts);
	s = "\"Mr.Watson\" <sip:watson@worcester.bell-telephone.com>;q=0.7; expires=3600,\"Mr.Watson\" <mailto:watson@bell-telephone.com> ;q=0.1";
	assert(0 == sip_header_contacts(s, s + strlen(s), &contacts) && 2 == sip_contacts_count(&contacts));
	c = sip_contacts_get(&contacts, 0);
	assert(0 == cstrcmp(&c->nickname, "Mr.Watson") && 0 == cstrcmp(&c->uri.scheme, "sip") && 0 == cstrcmp(&c->uri.host, "watson@worcester.bell-telephone.com") && 0 == sip_params_count(&c->uri.headers) && 0 == sip_params_count(&c->uri.parameters));
	assert(2 == sip_params_count(&c->params));
	assert(0 == cstrcmp(&sip_params_get(&c->params, 0)->name, "q") && 0 == cstrcmp(&sip_params_get(&c->params, 0)->value, "0.7"));
	assert(0 == cstrcmp(&sip_params_get(&c->params, 1)->name, "expires") && 0 == cstrcmp(&sip_params_get(&c->params, 1)->value, "3600"));
	assert(c->q == 0.7 && c->expires == 3600);
	c = sip_contacts_get(&contacts, 1);
	assert(0 == cstrcmp(&c->nickname, "Mr.Watson") && 0 == cstrcmp(&c->uri.scheme, "mailto") && 0 == cstrcmp(&c->uri.host, "watson@bell-telephone.com") && 0 == sip_params_count(&c->uri.headers) && 0 == sip_params_count(&c->uri.parameters));
	assert(1 == sip_params_count(&c->params) && 0 == cstrcmp(&sip_params_get(&c->params, 0)->name, "q") && 0 == cstrcmp(&sip_params_get(&c->params, 0)->value, "0.1"));
	assert(1 == sip_params_count(&c->params) && c->q == 0.1);
	sip_contacts_free(&contacts);

	s = "<sips:bob@192.0.2.4>;expires=60";
	assert(0 == sip_header_contact(s, s + strlen(s), &contact));
	assert(0 == contact.nickname.n && 0 == cstrcmp(&contact.uri.scheme, "sips") && 0 == cstrcmp(&contact.uri.host, "bob@192.0.2.4") && 0 == sip_params_count(&contact.uri.headers) && 0 == sip_params_count(&contact.uri.parameters));
	assert(1 == sip_params_count(&contact.params) && 0 == cstrcmp(&sip_params_get(&contact.params, 0)->name, "expires") && 0 == cstrcmp(&sip_params_get(&contact.params, 0)->value, "60"));
	assert(1 == sip_params_count(&contact.params) && contact.expires == 60);
	assert(sip_contact_write(&contact, p, p + sizeof(p)) < sizeof(p) && 0 == strcmp(s, p));
	sip_contact_params_free(&contact);

	s = "\"<sip:joe@big.org>\" <sip:joe@really.big.com>";
	assert(0 == sip_header_contact(s, s + strlen(s), &contact));
	assert(0 == cstrcmp(&contact.nickname, "<sip:joe@big.org>") && 0 == cstrcmp(&contact.uri.scheme, "sip") && 0 == cstrcmp(&contact.uri.host, "joe@really.big.com") && 0 == sip_params_count(&contact.uri.headers) && 0 == sip_params_count(&contact.uri.parameters));
	assert(sip_contact_write(&contact, p, p + sizeof(p)) < sizeof(p) && 0 == strcmp(s, p));
	sip_contact_params_free(&contact);
	
	s = "*";
	assert(0 == sip_header_contact(s, s + strlen(s), &contact));
	assert(sip_contact_write(&contact, p, p + sizeof(p)) < sizeof(p) && 0 == strcmp(s, p));
	sip_contact_params_free(&contact);

	// TO/FROM
	s = "Alice <sip:alice@atlanta.com>;tag=1928301774";
	assert(0 == sip_header_contact(s, s + strlen(s), &contact));
	assert(0 == cstrcmp(&contact.nickname, "Alice") && 0 == cstrcmp(&contact.uri.scheme, "sip") && 0 == cstrcmp(&contact.uri.host, "alice@atlanta.com") && 0 == sip_params_count(&contact.uri.headers) && 0 == sip_params_count(&contact.uri.parameters));
	assert(1 == sip_params_count(&contact.params) && 0 == cstrcmp(&sip_params_get(&contact.params, 0)->name, "tag") && 0 == cstrcmp(&sip_params_get(&contact.params, 0)->value, "1928301774"));
	assert(1 == sip_params_count(&contact.params) && 0 == cstrcmp(&contact.tag, "1928301774"));
	assert(sip_contact_write(&contact, p, p + sizeof(p)) < sizeof(p) && 0 == strcmp(s, p));
	sip_contact_params_free(&contact);

	s = "\"alice\" <sip:alice@192.168.1.10:63254>;+sip.instance=\"<urn:uuid:4bc9608d-8364-00c4-a871-10d41d9d2923>\";+org.linphone.specs=\"groupchat,lime\";pub-gruu=\"sip:alice@sip.linphone.org;gr=urn:uuid:4bc9608d-8364-00c4-a871-10d41d9d2923\",alice <sip:alice@192.168.1.11:40736;app-id=929724111839;pn-type=firebase;pn-timeout=0;pn-tok=dJ1uwpidM_0:APA91bF9KIATqa4LvyLWfpS83XB380FItpoIjYlUwYmRltm00hcBz7Dnb8N-xm943HVBmu8efwa5qxADOFsNd25xZlpKxlvsJSH3-WKQISwV8bCpSNAVdRJyRsggjSVmQmiD2wsQfg9d;pn-silent=1>;+sip.instance=\"<urn:uuid:3789dd53-f7ae-00b7-b6f8-0350e404d446>\";+org.linphone.specs=\"groupchat,lime\";pub-gruu=\"sip:alice@sip.linphone.org;gr=urn:uuid:3789dd53-f7ae-00b7-b6f8-0350e404d446\"";
	sip_contacts_init(&contacts);
	assert(0 == sip_header_contacts(s, s + strlen(s), &contacts) && 2 == sip_contacts_count(&contacts));
	c = sip_contacts_get(&contacts, 0);
	assert(0 == cstrcmp(&c->nickname, "alice") && 0 == cstrcmp(&c->uri.scheme, "sip") && 0 == cstrcmp(&c->uri.host, "alice@192.168.1.10:63254") && 0 == sip_params_count(&c->uri.headers) && 0 == sip_params_count(&c->uri.parameters));
	assert(3 == sip_params_count(&c->params) && 0 == cstrcmp(&sip_params_get(&c->params, 0)->name, "+sip.instance") && 0 == cstrcmp(&sip_params_get(&c->params, 0)->value, "\"<urn:uuid:4bc9608d-8364-00c4-a871-10d41d9d2923>\""));
	assert(0 == cstrcmp(&sip_params_get(&c->params, 1)->name, "+org.linphone.specs") && 0 == cstrcmp(&sip_params_get(&c->params, 1)->value, "\"groupchat,lime\""));
	assert(0 == cstrcmp(&sip_params_get(&c->params, 2)->name, "pub-gruu") && 0 == cstrcmp(&sip_params_get(&c->params, 2)->value, "\"sip:alice@sip.linphone.org;gr=urn:uuid:4bc9608d-8364-00c4-a871-10d41d9d2923\""));
	c = sip_contacts_get(&contacts, 1);
	assert(0 == cstrcmp(&c->nickname, "alice") && 0 == cstrcmp(&c->uri.scheme, "sip") && 0 == cstrcmp(&c->uri.host, "alice@192.168.1.11:40736") && 0 == sip_params_count(&c->uri.headers) && 5 == sip_params_count(&c->uri.parameters));
	assert(0 == cstrcmp(&sip_params_get(&c->uri.parameters, 0)->name, "app-id") && 0 == cstrcmp(&sip_params_get(&c->uri.parameters, 0)->value, "929724111839"));
	assert(0 == cstrcmp(&sip_params_get(&c->uri.parameters, 1)->name, "pn-type") && 0 == cstrcmp(&sip_params_get(&c->uri.parameters, 1)->value, "firebase"));
	assert(0 == cstrcmp(&sip_params_get(&c->uri.parameters, 2)->name, "pn-timeout") && 0 == cstrcmp(&sip_params_get(&c->uri.parameters, 2)->value, "0"));
	assert(0 == cstrcmp(&sip_params_get(&c->uri.parameters, 3)->name, "pn-tok") && 0 == cstrcmp(&sip_params_get(&c->uri.parameters, 3)->value, "dJ1uwpidM_0:APA91bF9KIATqa4LvyLWfpS83XB380FItpoIjYlUwYmRltm00hcBz7Dnb8N-xm943HVBmu8efwa5qxADOFsNd25xZlpKxlvsJSH3-WKQISwV8bCpSNAVdRJyRsggjSVmQmiD2wsQfg9d"));
	assert(0 == cstrcmp(&sip_params_get(&c->uri.parameters, 4)->name, "pn-silent") && 0 == cstrcmp(&sip_params_get(&c->uri.parameters, 4)->value, "1"));
	assert(3 == sip_params_count(&c->params) && 0 == cstrcmp(&sip_params_get(&c->params, 0)->name, "+sip.instance") && 0 == cstrcmp(&sip_params_get(&c->params, 0)->value, "\"<urn:uuid:3789dd53-f7ae-00b7-b6f8-0350e404d446>\""));
	assert(0 == cstrcmp(&sip_params_get(&c->params, 1)->name, "+org.linphone.specs") && 0 == cstrcmp(&sip_params_get(&c->params, 1)->value, "\"groupchat,lime\""));
	assert(0 == cstrcmp(&sip_params_get(&c->params, 2)->name, "pub-gruu") && 0 == cstrcmp(&sip_params_get(&c->params, 2)->value, "\"sip:alice@sip.linphone.org;gr=urn:uuid:3789dd53-f7ae-00b7-b6f8-0350e404d446\""));
	assert(sip_contact_write(c, p, p + sizeof(p)) < sizeof(p) && 0 == strcmp(s+227, p));
	sip_contacts_free(&contacts);
}
#endif
