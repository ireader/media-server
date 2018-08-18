#include "sip-message.h"
#include "sip-header.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#if defined(OS_WINDOWS)
	#if !defined(strcasecmp)
		#define strcasecmp	_stricmp
	#endif
#endif

struct sip_message_t* sip_message_create()
{
	struct sip_message_t* msg;
	msg = (struct sip_message_t*)malloc(sizeof(*msg) + 2 * 1024);
	if (NULL == msg)
		return NULL;

	memset(msg, 0, sizeof(*msg));
	msg->ptr.ptr = msg + 1;
	msg->ptr.end = msg->ptr.ptr + 2 * 1024;
	return msg;
}

int sip_message_destroy(struct sip_message_t* msg)
{
	sip_vias_free(&msg->vias);
	sip_contacts_free(&msg->contacts);
	sip_uris_free(&msg->routers);
	sip_uris_free(&msg->record_routers);
	sip_params_free(&msg->headers);
	free(msg);
	return 0;
}

static int sip_message_copy(struct sip_message_t* msg, struct cstring_t* str, const char* s)
{
	int n;
	n = msg->ptr.end - msg->ptr.ptr;

	str->p = msg->ptr.ptr;
	str->n = s ? strlen(s) : 0;
	str->n = str->n >= n ? str->n : n;

	memcpy(str->p, s, str->n);
	msg->ptr.ptr += str->n;
	return 0;
}

// Fill From/To/Call-Id/Max-Forwards/CSeq
int sip_message_init(struct sip_message_t* msg, const char* method, const char* from, const char* to, const char* callid)
{
	struct cstring_t f, t;
	sip_message_copy(msg, &t, to);
	sip_message_copy(msg, &f, from);
	sip_message_copy(msg, &msg->callid, callid);
	sip_message_copy(msg, &msg->cseq.method, method);
	if (0 != sip_header_contact(f.p, f.p + f.n, &msg->from)
		|| 0 != sip_header_contact(t.p, t.p + t.n, &msg->to))
		return -1;

	if (!cstrvalid(&msg->from.tag))
		sip_message_copy(msg, &msg->from.tag, tag);

	msg->cseq.id = rand();
	msg->maxforwards = SIP_MAX_FORWARDS;
	return 0;
}

int sip_message_isinvite(const struct sip_message_t* msg)
{
	return 0 == cstrcasecmp(&msg->cseq.method, SIP_METHOD_INVITE) ? 1 : 0;
}

int sip_message_load(struct sip_message_t* msg, const struct http_parser_t* parser)
{
	int i, r = 0;
	const char* name;
	const char* value;
	const char* end;
	struct sip_uri_t uri;
	struct sip_param_t param;

	http_get_version(parser, msg->u.s.protocol, &msg->u.s.vermajor, &msg->u.s.verminor);
	msg->u.s.code = http_get_status_code(parser);
	msg->u.s.reason.p = http_get_status_reason(parser);
	msg->u.s.reason.n = strlen(msg->u.s.reason.p);
	
	for (i = 0; i < http_get_header_count(parser) && 0 == r; i++)
	{
		if(0 != http_get_header(parser, i, &name, &value))
			continue;

		end = value + strlen(value);
		if (0 == strcasecmp(SIP_HEADER_FROM, name) || 0 == strcasecmp(SIP_HEADER_ABBR_FROM, name))
		{
			r = sip_header_contact(value, end, &msg->from);
		}
		else if(0 == strcasecmp(SIP_HEADER_TO, name) || 0 == strcasecmp(SIP_HEADER_ABBR_TO, name))
		{
			r = sip_header_contact(value, end, &msg->to);
		}
		else if (0 == strcasecmp(SIP_HEADER_CALLID, name) || 0 == strcasecmp(SIP_HEADER_ABBR_CALLID, name))
		{
			msg->callid.p = value;
			msg->callid.n = end - value;
		}
		else if (0 == strcasecmp(SIP_HEADER_CSEQ, name))
		{
			r = sip_header_cseq(value, end, &msg->cseq);
		}
		else if (0 == strcasecmp(SIP_HEADER_MAX_FORWARDS, name))
		{
			msg->maxforwards = strtol(value, NULL, 10);
		}
		else if (0 == strcasecmp(SIP_HEADER_VIA, name) || 0 == strcasecmp(SIP_HEADER_ABBR_VIA, name))
		{
			r = sip_header_via(value, end, &msg->vias);
		}
		else if (0 == strcasecmp(SIP_HEADER_CONTACT, name) || 0 == strcasecmp(SIP_HEADER_ABBR_CONTACT, name))
		{
			r = sip_header_contacts(value, end, &msg->contacts);
		}
		else if (0 == strcasecmp(SIP_HEADER_ROUTE, name))
		{
			r = sip_header_uri(value, end, &uri);
			if(0 == r)
				sip_uris_push(&msg->routers, &uri);
		}
		else if (0 == strcasecmp(SIP_HEADER_RECORD_ROUTE, name))
		{
			r = sip_header_uri(value, end, &uri);
			if (0 == r)
				sip_uris_push(&msg->record_routers, &uri);
		}
		else
		{
			// ignore
		}

		memset(&param, 0, sizeof(param));
		param.name.p = name;
		param.name.n = strlen(name);
		param.value.p = value;
		param.value.n = strlen(value);
		r = sip_params_push(&msg->headers, &param);
	}

	return r;
}

int sip_message_write(const struct sip_message_t* msg, uint8_t* data, int bytes)
{
	int i, n;
	int content_length;
	uint8_t* p, *end;
	const struct sip_param_t* param;

	p = data;
	end = data + bytes;
	content_length = 0;

	// Request-Line
	if (p < end) p += cstrcpy(&msg->u.c.method, p, end - p);
	if (p < end) *p++ = ' ';
	if (p < end) p += cstrcpy(&msg->u.c.uri.host, p, end - p);
	n = snprintf(p, end - p, " SIP/2.0");
	if (n < 0 || n >= end - p)
		return -ENOMEM; // don't have enough space

	// 6-base headers
	if (p < end) p += snprintf(p, end - p, "\r\n%s: ", SIP_HEADER_TO);
	if (p < end) p += sip_contact_write(&msg->to, p, end);
	if (p < end) p += snprintf(p, end - p, "\r\n%s: ", SIP_HEADER_FROM);
	if (p < end) p += sip_contact_write(&msg->to, p, end);
	if (p < end) p += snprintf(p, end - p, "\r\n%s: ", SIP_HEADER_CALLID);
	if (p < end) p += cstrcpy(&msg->callid, p, end - p);
	if (p < end) p += snprintf(p, end - p, "\r\n%s: ", SIP_HEADER_CSEQ);
	if (p < end) p += sip_cseq_write(&msg->cseq, p, end);
	if (p < end) p += snprintf(p, end - p, "\r\n%s: %u", SIP_HEADER_MAX_FORWARDS, msg->maxforwards);
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
	for (i = 0; i < sip_uris_count((struct sip_uris_t*)&msg->routers); i++)
	{
		if (p < end) p += snprintf(p, end - p, "\r\n%s: ", SIP_HEADER_ROUTE);
		if (p < end) p += sip_uri_write(sip_uris_get((struct sip_uris_t*)&msg->routers, i), p, end);
	}

	// record-routers
	for (i = 0; i < sip_uris_count((struct sip_uris_t*)&msg->record_routers); i++)
	{
		if (p < end) p += snprintf(p, end - p, "\r\n%s: ", SIP_HEADER_RECORD_ROUTE);
		if (p < end) p += sip_uri_write(sip_uris_get((struct sip_uris_t*)&msg->record_routers, i), p, end);
	}

	// other headers
	for (i = 0; i < sip_params_count((struct sip_params_t*)&msg->headers); i++)
	{
		param = sip_params_get((struct sip_params_t*)&msg->headers, i);
		if (!cstrvalid(&param->name) || !cstrvalid(&param->value))
			continue;

		if (0 == cstrcasecmp(&param->name, "Content-Length"))
			content_length = atoi(param->value.p); // has content length

		if (p < end) p += snprintf(p, end - p, "\r\n");
		if (p < end) p += cstrcpy(&param->name, p, end - p);
		if (p < end) p += snprintf(p, end - p, ": ");
		if (p < end) p += cstrcpy(&param->value, p, end - p);
	}

	if (p < end) p += snprintf(p, end - p, "\r\n\r\n"); // header end

	// payload
	assert(0 == content_length || msg->size == content_length);
	if (msg->payload && msg->size > 0)
	{
		if (0 == content_length && p < end)
		{
			p -= 2; // overwrite last \r\n
			p += snprintf(p, end - p, "Content-Length: %d\r\n\r\n", msg->size);
		}

		n = end - p > msg->size ? msg->size : end - p;
		memcpy(p, msg->payload, n);
		p += n;
	}
	
	return p - data;
}
