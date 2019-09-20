#include "sip-message.h"
#include "sip-header.h"
#include "sip-dialog.h"
#include "sys/system.h"
#include "cstringext.h"
#include "uuid.h"
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static void sip_message_copy(struct sip_message_t* msg, struct cstring_t* str, const char* s)
{
	msg->ptr.ptr = cstring_clone(msg->ptr.ptr, msg->ptr.end, str, s ? s : "", s ? strlen(s) : 0);
}
static void sip_message_copy2(struct sip_message_t* msg, struct cstring_t* str, const struct cstring_t* src)
{
	msg->ptr.ptr = cstring_clone(msg->ptr.ptr, msg->ptr.end, str, src->p, src->n);
}
static int sip_message_add_param(struct sip_params_t* params, const char* name, const struct cstring_t* value)
{
	struct sip_param_t param;
	param.name.p = name;
	param.name.n = strlen(name);
	param.value.p = value->p;
	param.value.n = value->n;
	return sip_params_push(params, &param);
}

struct sip_message_t* sip_message_create(int mode)
{
	struct sip_message_t* msg;
	msg = (struct sip_message_t*)malloc(sizeof(*msg) + 2 * 1024);
	if (NULL == msg)
		return NULL;

	memset(msg, 0, sizeof(*msg));
	msg->mode = mode;
	msg->ptr.ptr = (char*)(msg + 1);
	msg->ptr.end = msg->ptr.ptr + 2 * 1024;

	sip_vias_init(&msg->vias);
	sip_uris_init(&msg->routers);
	sip_uris_init(&msg->record_routers);
	sip_contacts_init(&msg->contacts);
	sip_params_init(&msg->headers);
	return msg;
}

int sip_message_destroy(struct sip_message_t* msg)
{
	if (msg)
	{
		sip_vias_free(&msg->vias);
		sip_uris_free(&msg->routers);
		sip_uris_free(&msg->record_routers);
		sip_contacts_free(&msg->contacts);
		sip_params_free(&msg->headers);
		free(msg);
	}
	return 0;
}

int sip_message_clone(struct sip_message_t* msg, const struct sip_message_t* clone)
{
	int i;
	struct sip_via_t via;
	struct sip_uri_t uri;
	struct sip_contact_t contact;
	struct sip_param_t header, *p;

	msg->mode = clone->mode;

	// 1. request uri
	if (clone->mode == SIP_MESSAGE_REQUEST)
	{
		sip_message_copy2(msg, &msg->u.c.method, &clone->u.c.method);
		msg->ptr.ptr = sip_uri_clone(msg->ptr.ptr, msg->ptr.end, &msg->u.c.uri, &clone->u.c.uri);
	}
	else
	{
		msg->u.s.code = clone->u.s.code;
		msg->u.s.verminor = clone->u.s.verminor;
		msg->u.s.vermajor = clone->u.s.vermajor;
		memcpy(msg->u.s.protocol, clone->u.s.protocol, sizeof(msg->u.s.protocol));
		sip_message_copy2(msg, &msg->u.s.reason, &clone->u.s.reason);
	}

	// 2. base headers
	msg->cseq.id = clone->cseq.id;
	msg->maxforwards = clone->maxforwards;
	sip_message_copy2(msg, &msg->cseq.method, &clone->cseq.method);
	sip_message_copy2(msg, &msg->callid, &clone->callid);
	msg->ptr.ptr = sip_contact_clone(msg->ptr.ptr, msg->ptr.end, &msg->to, &clone->to);
	msg->ptr.ptr = sip_contact_clone(msg->ptr.ptr, msg->ptr.end, &msg->from, &clone->from);
	
	for (i = 0; i < sip_vias_count(&clone->vias); i++)
	{
		msg->ptr.ptr = sip_via_clone(msg->ptr.ptr, msg->ptr.end, &via, sip_vias_get(&clone->vias, i));
		sip_vias_push(&msg->vias, &via);
	}

	for (i = 0; i < sip_contacts_count(&clone->contacts); i++)
	{
		msg->ptr.ptr = sip_contact_clone(msg->ptr.ptr, msg->ptr.end, &contact, sip_contacts_get(&clone->contacts, i));
		sip_contacts_push(&msg->contacts, &contact);
	}

	for (i = 0; i < sip_uris_count(&clone->routers); i++)
	{
		msg->ptr.ptr = sip_uri_clone(msg->ptr.ptr, msg->ptr.end, &uri, sip_uris_get(&clone->routers, i));
		sip_uris_push(&msg->routers, &uri);
	}

	for (i = 0; i < sip_uris_count(&clone->record_routers); i++)
	{
		msg->ptr.ptr = sip_uri_clone(msg->ptr.ptr, msg->ptr.end, &uri, sip_uris_get(&clone->record_routers, i));
		sip_uris_push(&msg->record_routers, &uri);
	}

	// 3. other headers
	for (i = 0; i < sip_params_count(&clone->headers); i++)
	{
		p = sip_params_get(&clone->headers, i);
		sip_message_copy2(msg, &header.name, &p->name);
		sip_message_copy2(msg, &header.value, &p->value);
		sip_params_push(&msg->headers, &header);
	}

	return 0;
}

// Fill From/To/Call-Id/Max-Forwards/CSeq
// Add Via by transport ???
int sip_message_init(struct sip_message_t* msg, const char* method, const char* uri, const char* from, const char* to)
{
	char tag[16];
	char callid[128];
	struct cstring_t u, f, t;
	struct sip_contact_t contact;

	memset(callid, 0, sizeof(callid));
	uuid_generate(callid); // TODO: callid @ host
	sip_message_copy(msg, &u, uri);
	sip_message_copy(msg, &t, to);
	sip_message_copy(msg, &f, from);
	sip_message_copy(msg, &msg->callid, callid);
	sip_message_copy(msg, &msg->cseq.method, method);
	if (0 != sip_header_contact(u.p, u.p + u.n, &contact)
		|| 0 != sip_header_contact(f.p, f.p + f.n, &msg->from)
		|| 0 != sip_header_contact(t.p, t.p + t.n, &msg->to))
		return -1;

	if (!cstrvalid(&msg->from.tag))
	{
        // TODO tag random
		snprintf(tag, sizeof(tag), "%u", (unsigned int)system_clock());
		sip_message_copy(msg, &msg->from.tag, tag);
		sip_message_add_param(&msg->from.params, "tag", &msg->from.tag);
	}

	// initialize remote target
	memcpy(&msg->u.c.method, &msg->cseq.method, sizeof(struct cstring_t));
	memcpy(&msg->u.c.uri, &contact.uri, sizeof(struct sip_uri_t));

	// TODO: Via

	// For non-REGISTER requests outside of a dialog, the sequence number 
	// value is arbitrary. The sequence number value MUST be expressible 
	// as a 32-bit unsigned integer and MUST be less than 2**31
	msg->cseq.id = rand();
	msg->maxforwards = SIP_MAX_FORWARDS;
	return 0;
}

// Copy From/To/Call-Id/Max-Forwards/CSeq
// Add Via by transport ???
int sip_message_init2(struct sip_message_t* msg, const char* method, const struct sip_dialog_t* dialog)
{
	int i;
	struct sip_uri_t uri;
	//struct cstring_t f, t;

	//f.p = msg->ptr.ptr;
	//f.n = sip_uri_write(&dialog->local.uri, msg->ptr.ptr, msg->ptr.end);
	//msg->ptr.ptr += f.n;
	//t.p = msg->ptr.ptr;
	//t.n = sip_uri_write(&dialog->remote.uri, msg->ptr.ptr, msg->ptr.end);
	//msg->ptr.ptr += t.n;

	//if (0 != sip_header_contact(f.p, f.p + f.n, &msg->from)
	//	|| 0 != sip_header_contact(t.p, t.p + t.n, &msg->to))
	//	return -1;

	msg->ptr.ptr = sip_contact_clone(msg->ptr.ptr, msg->ptr.end, &msg->from, &dialog->local.uri);
	msg->ptr.ptr = sip_contact_clone(msg->ptr.ptr, msg->ptr.end, &msg->to, &dialog->remote.uri);
	sip_message_copy(msg, &msg->cseq.method, method);
	sip_message_copy2(msg, &msg->callid, &dialog->callid);
	//sip_message_copy(msg, &msg->to.tag, dialog->remote.tag);
	//sip_message_copy(msg, &msg->from.tag, dialog->local.tag);

	// initialize routers
	for (i = 0; i < sip_uris_count(&dialog->routers); i++)
	{
		msg->ptr.ptr = sip_uri_clone(msg->ptr.ptr, msg->ptr.end, &uri, sip_uris_get(&dialog->routers, i));
		sip_uris_push(&msg->routers, &uri);
	}

	// initialize remote target
	memcpy(&msg->u.c.method, &msg->cseq.method, sizeof(struct cstring_t));
	memcpy(&msg->u.c.uri, &dialog->target, sizeof(struct sip_uri_t));

	// TODO: Via

	msg->cseq.id = dialog->local.id;
	msg->maxforwards = SIP_MAX_FORWARDS;
	return 0;
}

// Copy From/To/Call-Id/Max-Forwards/CSeq
// Add Via by transport ???
int sip_message_init3(struct sip_message_t* msg, const struct sip_message_t* req)
{
	int i;
	char tag[16];
	struct sip_uri_t uri;
	struct sip_via_t via;

	// 8.2.6 Generating the Response
	// 8.2.6.2 Headers and Tags
	// 1. The From field of the response MUST equal the From header field of the request
	msg->ptr.ptr = sip_contact_clone(msg->ptr.ptr, msg->ptr.end, &msg->from, &req->from);

	// 2. The Call-ID header field of the response MUST equal the Call-ID header field of the request.
	sip_message_copy2(msg, &msg->callid, &req->callid);

	// 3. The CSeq header field of the response MUST equal the CSeq field of the request.
	sip_message_copy2(msg, &msg->cseq.method, &req->cseq.method);
	msg->cseq.id = req->cseq.id;

	// 4. The Via header field values in the response MUST equal the Via header field values 
	//	  in the request and MUST maintain the same ordering
	for (i = 0; i < sip_vias_count(&req->vias); i++)
	{
		msg->ptr.ptr = sip_via_clone(msg->ptr.ptr, msg->ptr.end, &via, sip_vias_get(&req->vias, i));
		sip_vias_push(&msg->vias, &via);
	}

	// 5. If a request contained a To tag in the request, the To header field
	//	  in the response MUST equal that of the request. However, if the To
	//	  header field in the request did not contain a tag, the URI in the To
	//	  header field in the response MUST equal the URI in the To header field; 
	//	  additionally, the UAS MUST add a tag to the To header field in the response 
	//	  (with the exception of the 100 (Trying) response, in which a tag MAY be present)
	//	  The same tag MUST be used for all responses to that request, both final
	//	  and provisional (again excepting the 100 (Trying)).
	msg->ptr.ptr = sip_contact_clone(msg->ptr.ptr, msg->ptr.end, &msg->to, &req->to);
	if (!cstrvalid(&msg->to.tag))
	{
        // TODO tag random
		snprintf(tag, sizeof(tag), "%u", (unsigned int)system_clock());
		sip_message_copy(msg, &msg->to.tag, tag);
		sip_message_add_param(&msg->to.params, "tag", &msg->to.tag);
	}

	// 6. Max-Forwards
	msg->maxforwards = SIP_MAX_FORWARDS;

	// 12.1.1 UAS behavior (p70)
	// When a UAS responds to a request with a response that establishes a
	// dialog (such as a 2xx to INVITE), the UAS MUST copy all Record-Route
	// header field values from the request into the response (including the
	// URIs, URI parameters, and any Record-Route header field parameters,
	// whether they are known or unknown to the UAS) and MUST maintain the
	// order of those values.
	for (i = 0; i < sip_uris_count(&req->record_routers); i++)
	{
		msg->ptr.ptr = sip_uri_clone(msg->ptr.ptr, msg->ptr.end, &uri, sip_uris_get(&req->record_routers, i));
		sip_uris_push(&msg->record_routers, &uri);
	}

	// 12.1.1 UAS behavior (p70)
	// The UAS MUST add a Contact header field to the response.


	// initialize routers
	//for (i = 0; i < sip_uris_count(&req->routers); i++)
	//{
	//	msg->ptr.ptr = sip_via_clone(msg->ptr.ptr, msg->ptr.end, &uri, sip_uris_get(&req->routers, i));
	//	sip_uris_push(&msg->routers, &via);
	//}

	return 0;
}

int sip_message_initack(struct sip_message_t* ack, const struct sip_message_t* origin)
{
	struct sip_via_t via;
	assert(SIP_MESSAGE_REQUEST == ack->mode);

	// 1. request uri
	sip_message_copy2(ack, &ack->u.c.method, &origin->u.c.method);
	ack->ptr.ptr = sip_uri_clone(ack->ptr.ptr, ack->ptr.end, &ack->u.c.uri, &origin->u.c.uri);

	// 2. base headers
	ack->cseq.id = origin->cseq.id;
	ack->maxforwards = origin->maxforwards;
	sip_message_copy2(ack, &ack->cseq.method, &origin->cseq.method);
	sip_message_copy2(ack, &ack->callid, &origin->callid);
	ack->ptr.ptr = sip_contact_clone(ack->ptr.ptr, ack->ptr.end, &ack->to, &origin->to);
	ack->ptr.ptr = sip_contact_clone(ack->ptr.ptr, ack->ptr.end, &ack->from, &origin->from);

	// 3. via
	if (sip_vias_count(&origin->vias) > 0)
	{
		ack->ptr.ptr = sip_via_clone(ack->ptr.ptr, ack->ptr.end, &via, sip_vias_get(&origin->vias, 0));
		sip_vias_push(&ack->vias, &via);
	}

	return 0;
}

int sip_message_isinvite(const struct sip_message_t* msg)
{
	return 0 == cstrcasecmp(&msg->cseq.method, SIP_METHOD_INVITE) ? 1 : 0;
}

int sip_message_isregister(const struct sip_message_t* msg)
{
	return 0 == cstrcasecmp(&msg->cseq.method, SIP_METHOD_REGISTER) ? 1 : 0;
}

int sip_message_isack(const struct sip_message_t* msg)
{
	return 0 == cstrcasecmp(&msg->cseq.method, SIP_METHOD_ACK) ? 1 : 0;
}

int sip_message_isbye(const struct sip_message_t* msg)
{
	return 0 == cstrcasecmp(&msg->cseq.method, SIP_METHOD_BYE) ? 1 : 0;
}

int sip_message_iscancel(const struct sip_message_t* msg)
{
	return 0 == cstrcasecmp(&msg->cseq.method, SIP_METHOD_CANCEL) ? 1 : 0;
}

int sip_message_isrefer(const struct sip_message_t* msg)
{
	return 0 == cstrcasecmp(&msg->cseq.method, SIP_METHOD_REFER) ? 1 : 0;
}

int sip_message_isnotify(const struct sip_message_t* msg)
{
	return 0 == cstrcasecmp(&msg->cseq.method, SIP_METHOD_NOTIFY) ? 1 : 0;
}

int sip_message_issubscribe(const struct sip_message_t* msg)
{
	return 0 == cstrcasecmp(&msg->cseq.method, SIP_METHOD_SUBSCRIBE) ? 1 : 0;
}

int sip_message_load(struct sip_message_t* msg, const struct http_parser_t* parser)
{
	int i, r;
	const char* name;
	const char* value;
	struct cstring_t param;

	r = http_get_version(parser, msg->u.s.protocol, &msg->u.s.vermajor, &msg->u.s.verminor);
	if (SIP_MESSAGE_REQUEST == msg->mode)
	{
		sip_message_copy(msg, &param, http_get_request_uri(parser));
		r = sip_header_uri(param.p, param.p + param.n, &msg->u.c.uri);
		if (0 != r) return r;
		sip_message_copy(msg, &msg->u.c.method, http_get_request_method(parser));
	}
	else
	{
		assert(SIP_MESSAGE_REPLY == msg->mode);
		msg->u.s.code = http_get_status_code(parser);
		sip_message_copy(msg, &msg->u.s.reason, http_get_status_reason(parser));
	}
	
	for (i = 0; i < http_get_header_count(parser) && 0 == r; i++)
	{
		if(0 != http_get_header(parser, i, &name, &value))
			continue;

		r = sip_message_add_header(msg, name, value);
	}

	msg->size = http_get_content_length(parser);
	msg->payload = http_get_content(parser);
	return r;
}

int sip_message_set_uri(struct sip_message_t* msg, const char* host)
{
	struct cstring_t uri;
	sip_message_copy(msg, &uri, host);
	return sip_header_uri(uri.p, uri.p + uri.n, &msg->u.c.uri);
}

const struct sip_uri_t* sip_message_get_next_hop(const struct sip_message_t* msg)
{
	const struct sip_uri_t *router;

	// 8.1.2 Sending the Request (p41)
	// 1. If the first element in the route set indicated a strict router (resulting
	//    in forming the request as described in Section 12.2.1.1), the procedures 
	//    MUST be applied to the Request-URI of the request.
	// 2. Otherwise, the procedures are applied to the first Route header field
	//	  value in the request (if one exists), or to the request's Request-URI
	//    if there is no Route header field present.

	// 18.1.1 Sending Requests (p142)
	// 1. A client that sends a request to a multicast address MUST add the
	// 	  "maddr" parameter to its Via header field value containing the
	// 	  destination multicast address, and for IPv4, SHOULD add the "ttl"
	// 	  parameter with a value of 1. Usage of IPv6 multicast is not defined
	// 	  in this specification
	// 2. Before a request is sent, the client transport MUST insert a value of
	// 	  the "sent-by" field into the Via header field.

	router = sip_uris_get(&msg->routers, 0);
	if (!router || router->lr)
		router = &msg->u.c.uri;
	return router;
}

// SIP/2.0 200 OK
static char* sip_message_status_line(const struct sip_message_t* msg, char* p, const char *end)
{
	p += snprintf(p, end - p, "%s/2.0 %3d ", msg->u.s.protocol[0] ? msg->u.s.protocol : "SIP", msg->u.s.code);
	if (p < end) p += cstrcpy(&msg->u.s.reason, p, end - p);
	return p;
}

// INVITE sip:bob@biloxi.com SIP/2.0
static char* sip_message_request_uri(const struct sip_message_t* msg, char* p, const char *end)
{
	int n;
	const char* phost;
	struct sip_uri_t uri;
	const struct sip_uri_t* host;
	const struct sip_uri_t *router;

	// 8.1.1.1 Request-URI (p35)
	// 1. The initial Request-URI of the message SHOULD be set to the value of
	//    the URI in the To field. 
	// 2. One notable exception is the REGISTER method; behavior for setting 
	//    the Request-URI of REGISTER is given in Section 10.
	// 3. When a pre-existing route set is present, the procedures for
	//    populating the Request-URI and Route header field detailed in Section
	//    12.2.1.1 MUST be followed (even though there is no dialog), using the
	//    desired Request-URI as the remote target URI.

	// 12.2.1.1 Generating the Request (p73)
	// 1. If the route set is empty, the UAC MUST place the remote target URI into 
	//    the Request-URI. The UAC MUST NOT add a Route header field to the request.
	// 2. If the route set is not empty, and the first URI in the route set contains 
	//    the lr parameter, the UAC MUST place the remote target URI into the 
	//    Request-URI and MUST include a Route header field containing the route 
	//    set values in order, including all parameters.
	// 3. If the route set is not empty, and its first URI does not contain the
	//    lr parameter, the UAC MUST place the first URI from the route set into 
	//    the Request-URI, stripping any parameters that are not allowed in a Request-URI.
	//    The UAC MUST add a Route header field containing the remainder of the 
	//	  route set values in order, including all parameters. The UAC MUST then 
	//    place the remote target URI into the Route header field as the last value.
	//	  METHOD sip:proxy1
	//	  Route: <sip:proxy2>,<sip:proxy3;lr>,<sip:proxy4>,<sip:user@remoteua>
	// 4. A UAC SHOULD include a Contact header field in any target refresh
	//    requests within a dialog, and unless there is a need to change it,

	router = sip_uris_get(&msg->routers, 0);
	if (!router || router->lr)
		host = &msg->u.c.uri;
	else
		host = router;

	// TODO: uri method (19.1 SIP and SIPS Uniform Resource Indicators)
	// sip:atlanta.com;method=REGISTER?to=alice%40atlanta.com
	if (p < end) p += cstrcpy(&msg->u.c.method, p, end - p);
	if (p < end) *p++ = ' ';
	
	if (0 != cstrcasecmp(&msg->u.c.method, SIP_METHOD_REGISTER))
	{
		// INVITE sip:bob@biloxi.com SIP/2.0
		if (p < end) p += sip_request_uri_write(host, p, end);
	}
	else
	{
		// The Request-URI names the domain of the location service for 
		// which the registration is meant (for example, "sip:chicago.com"). 
		// The "userinfo" and "@" components of the SIP URI MUST NOT be present.
		
		memcpy(&uri, host, sizeof(uri));
		phost = cstrchr(&uri.host, '@');
		if (phost)
		{
			uri.host.n -= ++phost - uri.host.p;
			uri.host.p = phost;
		}

		// REGISTER sip:registrar.biloxi.com SIP/2.0
		if (p < end) p += sip_request_uri_write(&uri, p, end);
	}

	n = snprintf(p, end - p, " SIP/2.0");
	if (n < 0 || n >= end - p)
		return (char*)end; // don't have enough space
	return p + n;
}

static char* sip_message_routers(const struct sip_message_t* msg, char* p, const char *end)
{
	int i, strict_router;
	const struct sip_uri_t *router;

	router = sip_uris_get(&msg->routers, 0);
	strict_router = (!router || router->lr) ? 0 : 1;

	// METHOD sip:proxy1
	// Route: <sip:proxy2>,<sip:proxy3;lr>,<sip:proxy4>,<sip:user@remoteua>
	for (i = strict_router; i < sip_uris_count((struct sip_uris_t*)&msg->routers); i++)
	{
		if (p < end) p += snprintf(p, end - p, "\r\n%s: ", SIP_HEADER_ROUTE);
		if (p < end) p += sip_uri_write(sip_uris_get((struct sip_uris_t*)&msg->routers, i), p, end);
	}

	// place the remote target URI into the Route header field as the last value
	if (strict_router)
	{
		if (p < end) p += snprintf(p, end - p, "\r\n%s: ", SIP_HEADER_ROUTE);
		if (p < end) p += sip_uri_write(&msg->u.c.uri, p, end);
	}

	return p;
}

static inline int sip_message_skip_header(const struct cstring_t* name);
int sip_message_write(const struct sip_message_t* msg, uint8_t* data, int bytes)
{
	int i, n;
	int content_length;
	char* p, *end;
	const struct sip_param_t* param;
	
	// check overwrite
	assert(data + bytes <= (uint8_t*)msg || data >= (uint8_t*)msg->ptr.end);

	p = (char*)data;
	end = p + bytes;
	content_length = 0;

	// Request-Line
	if(SIP_MESSAGE_REQUEST == msg->mode)
		p = sip_message_request_uri(msg, p, end);
	else
		p = sip_message_status_line(msg, p, end);

	// 6-base headers
	if (p < end) p += snprintf(p, end - p, "\r\n%s: ", SIP_HEADER_TO);
	if (p < end) p += sip_contact_write(&msg->to, p, end);
	if (p < end) p += snprintf(p, end - p, "\r\n%s: ", SIP_HEADER_FROM);
	if (p < end) p += sip_contact_write(&msg->from, p, end);
	if (p < end) p += snprintf(p, end - p, "\r\n%s: %.*s", SIP_HEADER_CALLID, (int)msg->callid.n, msg->callid.p);
	if (p < end) p += snprintf(p, end - p, "\r\n%s: ", SIP_HEADER_CSEQ);
	if (p < end) p += sip_cseq_write(&msg->cseq, p, end);
	if (p < end) p += snprintf(p, end - p, "\r\n%s: %d", SIP_HEADER_MAX_FORWARDS, msg->maxforwards);
	for (i = 0; i < sip_vias_count((struct sip_vias_t*)&msg->vias); i++)
	{
		if (p < end) p += snprintf(p, end - p, "\r\n%s: ", SIP_HEADER_VIA);
		if (p < end) p += sip_via_write(sip_vias_get((struct sip_vias_t*)&msg->vias, i), p, end);
	}

	// contacts
	for (i = 0; i < sip_contacts_count((struct sip_contacts_t*)&msg->contacts); i++)
	{
		if (p < end) p += snprintf(p, end - p, "\r\n%s: ", SIP_HEADER_CONTACT);
		if (p < end) p += sip_contact_write(sip_contacts_get((struct sip_contacts_t*)&msg->contacts, i), p, end);
	}

	// routers
	p = sip_message_routers(msg, p, end);

	// record-routers
	for (i = 0; i < sip_uris_count((struct sip_uris_t*)&msg->record_routers); i++)
	{
		if (p < end) p += snprintf(p, end - p, "\r\n%s: ", SIP_HEADER_RECORD_ROUTE);
		if (p < end) p += sip_uri_write(sip_uris_get((struct sip_uris_t*)&msg->record_routers, i), p, end);
	}

	// PRACK
	if (0 != msg->rseq && p < end)
		p += snprintf(p, end - p, "\r\n%s: %u", SIP_HEADER_RSEQ, (unsigned int)msg->rseq);

	// INFO: recv-info/info-package
	if (cstrvalid(&msg->recv_info) && p < end)
		p += snprintf(p, end - p, "\r\n%s: %.*s", SIP_HEADER_RECV_INFO, (int)msg->recv_info.n, msg->recv_info.p);
	if (cstrvalid(&msg->info_package) && p < end)
		p += snprintf(p, end - p, "\r\n%s: %.*s", SIP_HEADER_INFO_PACKAGE, (int)msg->info_package.n, msg->info_package.p);

	// Subscribe/Notify
	if (cstrvalid(&msg->event) && p < end)
		p += snprintf(p, end - p, "\r\n%s: %.*s", SIP_HEADER_EVENT, (int)msg->event.n, msg->event.p);
	if (cstrvalid(&msg->allow_events) && p < end)
		p += snprintf(p, end - p, "\r\n%s: %.*s", SIP_HEADER_ALLOW_EVENTS, (int)msg->allow_events.n, msg->allow_events.p);
	if (cstrvalid(&msg->substate.state))
	{
		if (p < end) p += snprintf(p, end - p, "\r\n%s: ", SIP_HEADER_SUBSCRIBE_STATE);
		if (p < end) p += sip_substate_write(&msg->substate, p, end);
	}

	// other headers
	for (i = 0; i < sip_params_count((struct sip_params_t*)&msg->headers); i++)
	{
		param = sip_params_get((struct sip_params_t*)&msg->headers, i);
		if (!cstrvalid(&param->name) || !cstrvalid(&param->value))
			continue;

		if(sip_message_skip_header(&param->name))
			continue;

		if (0 == cstrcasecmp(&param->name, "Content-Length") || 0 == cstrcasecmp(&param->name, SIP_HEADER_ABBR_CONTENT_LENGTH))
		{
			assert(msg->size == atoi(param->value.p));
			content_length = 1; // has content length
		}

		if (p < end)
			p += snprintf(p, end - p, "\r\n%.*s: %.*s", (int)param->name.n, param->name.p, (int)param->value.n, param->value.p);
	}

	// add Content-Length header
	if (0 == content_length && p < end)
		p += snprintf(p, end - p, "\r\nContent-Length: %d", msg->size);

	// header end
	if (p < end) p += snprintf(p, end - p, "\r\n\r\n");

	// payload
	if (msg->size > 0 && p < end)
	{
		n = end - p > msg->size ? msg->size : (int)(end - p);
		memcpy(p, msg->payload, n);
		p += n;
	}
	
	return (int)((uint8_t*)p - data);
}

static inline int sip_message_skip_header(const struct cstring_t* name)
{
	int i;
	const char* s_headers[] = {
		SIP_HEADER_FROM, SIP_HEADER_TO, SIP_HEADER_CALLID, SIP_HEADER_CSEQ, SIP_HEADER_MAX_FORWARDS, SIP_HEADER_VIA,
		SIP_HEADER_CONTACT, SIP_HEADER_ROUTE, SIP_HEADER_RECORD_ROUTE, SIP_HEADER_RSEQ, SIP_HEADER_RECV_INFO,
		SIP_HEADER_INFO_PACKAGE, SIP_HEADER_EVENT, SIP_HEADER_ALLOW_EVENTS, SIP_HEADER_SUBSCRIBE_STATE, //SIP_HEADER_REFER_TO,
		SIP_HEADER_ABBR_FROM, SIP_HEADER_ABBR_TO, SIP_HEADER_ABBR_CALLID, SIP_HEADER_ABBR_VIA, SIP_HEADER_ABBR_CONTACT, //SIP_HEADER_ABBR_REFER_TO,
	};

	for (i = 0; i < sizeof(s_headers) / sizeof(s_headers[0]); i++)
	{
		if (0 == cstrcasecmp(name, s_headers[i]))
			return 1;
	}
	return 0;
}

int sip_message_add_header(struct sip_message_t* msg, const char* name, const char* value)
{
	int r;
	struct sip_uri_t uri;
	struct sip_param_t header;

	if (!name || !*name)
		return -1; // EINVAL

	sip_message_copy(msg, &header.name, name);
	sip_message_copy(msg, &header.value, value ? value : "");
	r = sip_params_push(&msg->headers, &header);
	if (0 != r)
		return r;

	if (0 == strcasecmp(SIP_HEADER_FROM, name) || 0 == strcasecmp(SIP_HEADER_ABBR_FROM, name))
	{
		r = sip_header_contact(header.value.p, header.value.p + header.value.n, &msg->from);
	}
	else if (0 == strcasecmp(SIP_HEADER_TO, name) || 0 == strcasecmp(SIP_HEADER_ABBR_TO, name))
	{
		r = sip_header_contact(header.value.p, header.value.p + header.value.n, &msg->to);
	}
	else if (0 == strcasecmp(SIP_HEADER_CALLID, name) || 0 == strcasecmp(SIP_HEADER_ABBR_CALLID, name))
	{
		msg->callid.p = header.value.p;
		msg->callid.n = header.value.n;
	}
	else if (0 == strcasecmp(SIP_HEADER_CSEQ, name))
	{
		r = sip_header_cseq(header.value.p, header.value.p + header.value.n, &msg->cseq);
	}
	else if (0 == strcasecmp(SIP_HEADER_MAX_FORWARDS, name))
	{
		msg->maxforwards = (int)strtol(value, NULL, 10);
	}
	else if (0 == strcasecmp(SIP_HEADER_VIA, name) || 0 == strcasecmp(SIP_HEADER_ABBR_VIA, name))
	{
		r = sip_header_vias(header.value.p, header.value.p + header.value.n, &msg->vias);
	}
	else if (0 == strcasecmp(SIP_HEADER_CONTACT, name) || 0 == strcasecmp(SIP_HEADER_ABBR_CONTACT, name))
	{
		r = sip_header_contacts(header.value.p, header.value.p + header.value.n, &msg->contacts);
	}
	else if (0 == strcasecmp(SIP_HEADER_ROUTE, name))
	{
		memset(&uri, 0, sizeof(uri));
		r = sip_header_uri(header.value.p, header.value.p + header.value.n, &uri);
		if (0 == r)
			sip_uris_push(&msg->routers, &uri);
	}
	else if (0 == strcasecmp(SIP_HEADER_RECORD_ROUTE, name))
	{
		memset(&uri, 0, sizeof(uri));
		r = sip_header_uri(header.value.p, header.value.p + header.value.n, &uri);
		if (0 == r)
			sip_uris_push(&msg->record_routers, &uri);
	}
	else if (0 == strcasecmp(SIP_HEADER_RECV_INFO, name))
	{
		msg->recv_info.p = header.value.p;
		msg->recv_info.n = header.value.n;
	}
	else if (0 == strcasecmp(SIP_HEADER_INFO_PACKAGE, name))
	{
		msg->info_package.p = header.value.p;
		msg->info_package.n = header.value.n;
	}
	else if (0 == strcasecmp(SIP_HEADER_EVENT, name))
	{
		msg->event.p = header.value.p;
		msg->event.n = header.value.n;
	}
	else if (0 == strcasecmp(SIP_HEADER_ALLOW_EVENTS, name))
	{
		msg->allow_events.p = header.value.p;
		msg->allow_events.n = header.value.n;
	}
	else if (0 == strcasecmp(SIP_HEADER_SUBSCRIBE_STATE, name))
	{
		memset(&msg->substate, 0, sizeof(msg->substate));
		r = sip_header_substate(header.value.p, header.value.p + header.value.n, &msg->substate);
	}
	else if (0 == strcasecmp(SIP_HEADER_RSEQ, name))
	{
		msg->rseq = atoi(header.value.p);
	}
	else
	{
		// do nothing
	}

	return r;
}

int sip_message_add_header_int(struct sip_message_t* msg, const char* name, int value)
{
	char v[32];
	snprintf(v, sizeof(v), "%d", value);
	return sip_message_add_header(msg, name, v);
}

int sip_message_get_header_count(const struct sip_message_t* msg)
{
	return sip_params_count(&msg->headers);
}

int sip_message_get_header(const struct sip_message_t* msg, int i, struct cstring_t* const name, struct cstring_t* const value)
{
	const struct sip_param_t* param;
	param = sip_params_get(&msg->headers, i);
	if (!param) return -1;
	memcpy(name, &param->name, sizeof(struct cstring_t));
	memcpy(value, &param->value, sizeof(struct cstring_t));
	return 0;
}

const struct cstring_t* sip_message_get_header_by_name(const struct sip_message_t* msg, const char* name)
{
	return sip_params_find_string(&msg->headers, name, (int)strlen(name));
}

int sip_message_set_reply_default_contact(struct sip_message_t* reply)
{
    struct sip_via_t* via;
    struct sip_contact_t contact;
    
    assert(SIP_MESSAGE_REPLY == reply->mode);
    memset(&contact, 0, sizeof(contact));
    
    // copy from TO
    sip_contact_clone(reply->ptr.ptr, reply->ptr.end, &contact, &reply->to);
    contact.tag.p = NULL; contact.tag.n = 0; // clear TO tag
    
    // update contact host by via.received
    via = sip_vias_get(&reply->vias, 0);
    if (via && cstrvalid(&via->received))
        sip_message_copy2(reply, &contact.uri.host, &via->received);
    
    return sip_contacts_push(&reply->contacts, &contact);
}
