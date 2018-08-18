#include "sip-uas-transaction.h"
#include "http-header-expires.h"
#include "uri-parse.h"

/*
REGISTER sip:registrar.biloxi.com SIP/2.0
Via: SIP/2.0/UDP bobspc.biloxi.com:5060;branch=z9hG4bKnashds7
Max-Forwards: 70
To: Bob <sip:bob@biloxi.com>
From: Bob <sip:bob@biloxi.com>;tag=456248
Call-ID: 843817637684230@998sdasdh09
CSeq: 1826 REGISTER
Contact: <sip:bob@192.0.2.4>
Expires: 7200
Content-Length: 0

SIP/2.0 200 OK
Via: SIP/2.0/UDP bobspc.biloxi.com:5060;branch=z9hG4bKnashds7;received=192.0.2.4
To: Bob <sip:bob@biloxi.com>;tag=2493k59kd
From: Bob <sip:bob@biloxi.com>;tag=456248
Call-ID: 843817637684230@998sdasdh09
CSeq: 1826 REGISTER
Contact: <sip:bob@192.0.2.4>
Expires: 7200
Content-Length: 0
*/

static int sip_uas_get_expires(const char* expires)
{
	struct tm tm;
	memset(&tm, 0, sizeof(tm));

	assert(expires);
	if (0 != http_header_expires(expires, &tm))
		return 0;

	return mktime(&tm) - time(NULL);
}

// 10.3 Processing REGISTER Requests(p63)
int sip_uas_onregister(struct sip_uas_transaction_t* t, const struct sip_message_t* req)
{
	int i, r, expires;
	struct uri_t* to;
	struct uri_t* uri;
	const char* header;
	struct sip_contact_t* contact;

	// If contact.expire is not provided, 
	// the value of the Expires header field is used instead
	header = http_get_header_by_name(t->http, "Expires");
	expires = header ? atoi(header) : 0;

	// 1. Request-URI

	// Request-URI: The "userinfo" and "@" components of the SIP URI MUST NOT be present
	uri = uri_parse(req->u.c.uri.host.p, req->u.c.uri.host.n);
	if (!uri || !uri->host)
	{
		return sip_uas_transaction_noninvite_reply(t, 400/*Invalid Request*/, NULL, 0);
	}
	assert(NULL == uri->userinfo && uri->host);

	// TODO: check domain and proxy to another host
	//if (0 != strcasecmp(uri->host, t->uas->domain))
	//{
	//}

	// 2. the registrar MUST process the Require header field values
	header = http_get_header_by_name(t->http, "require");
	if (!header)
	{
		// TODO: check require
	}

	// 3. authentication
	// 4. authorized modify registrations(403 Forbidden)

	// 5. To domain check (404 Not Found)
	to = uri_parse(req->to.uri.host.p, req->to.uri.host.n);
	if (!to || !to->host || 0 != strcasecmp(to->host, uri->host))
	{
		// all URI parameters MUST be removed (including the user-param), and
		// any escaped characters MUST be converted to their unescaped form.
		return sip_uas_transaction_noninvite_reply(t, 404/*Not Found*/, NULL, 0);
	}

	// 6. Contact
	//    * - multi-contacts, expires != 0 (400 Invalid Request)
	//    call-id
	//    cseq
	if (sip_contacts_match_any(&req->contacts) && (1 != sip_contacts_count(&req->contacts) || 0 < expires) )
	{
		return sip_uas_transaction_noninvite_reply(t, 400/*Invalid Request*/, NULL, 0);
	}

	// All registrations from a UAC SHOULD use the same Call-ID header 
	// field value for registrations sent to a particular registrar.
	req->callid;

	// A UA MUST increment the CSeq value by one for each
	// REGISTER request with the same Call-ID.
	assert(0 == cstrcasecmp(&req->cseq.method, "REGISTER"));
	req->cseq.id;

	// zero or more values containing address bindings
	req->contacts;

	// The Record-Route header field has no meaning in REGISTER 
	// requests or responses, and MUST be ignored if present.

	return t->uas->handler->onregister(t->uas->param, t, &req->to, &req->contacts);
	
	//if (423/*Interval Too Brief*/ == r)
	//{
	//	sip_uas_add_header_int(t, "Min-Expires", t->uas->min_expires_seconds);
	//}

	//// The Record-Route header field has no meaning in REGISTER requests or responses, 
	//// and MUST be ignored if present.
	//return sip_uas_transaction_noninvite_reply(t, r, NULL, 0);
}
