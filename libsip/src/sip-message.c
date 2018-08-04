#include "sip-message.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#if defined(OS_WINDOWS)
	#if !defined(strcasecmp)
		#define strcasecmp	_stricmp
	#endif
#endif

struct sip_message_t* sip_message_create(const uint8_t* data, int bytes)
{
	struct sip_message_t* msg;
	msg = (struct sip_message_t*)malloc(sizeof(*msg) + bytes);
	if (NULL == msg)
		return NULL;

	memcpy(msg + 1, data, bytes);
	return msg;
}

int sip_message_destroy(struct sip_message_t* msg)
{
	free(msg);
	return 0;
}

int sip_message_isinvite(const struct sip_message_t* msg)
{
	return 0 == cstrcasecmp(&msg->cseq.method, "INVITE") ? 1 : 0;
}

int sip_message_load(const struct http_parser_t* parser, struct sip_message_t* msg)
{
	int i, r = 0;
	const char* name;
	const char* value;
	const char* end;
	struct sip_uri_t uri;

	http_get_version(parser, msg->u.s.protocol, &msg->u.s.vermajor, &msg->u.s.verminor);
	msg->u.s.code = http_get_status_code(parser);
	msg->u.s.reason.p = http_get_status_reason(parser);
	msg->u.s.reason.n = strlen(msg->u.s.reason.p);
	
	for (i = 0; i < http_get_header_count(parser) && 0 == r; i++)
	{
		if(0 != http_get_header(parser, i, &name, &value))
			continue;

		end = value + strlen(value);
		if (0 == strcasecmp("From", name))
		{
			r = sip_header_contact(value, end, &msg->from);
		}
		else if(0 == strcasecmp("To", name))
		{
			r = sip_header_contact(value, end, &msg->to);
		}
		else if (0 == strcasecmp("Call-ID", name))
		{
			msg->callid.p = value;
			msg->callid.n = end - value;
		}
		else if (0 == strcasecmp("CSeq", name))
		{
			r = sip_header_cseq(value, end, &msg->cseq);
		}
		else if (0 == strcasecmp("Max-Forwards", name))
		{
			msg->maxforwards = strtol(value, NULL, 10);
		}
		else if (0 == strcasecmp("Via", name))
		{
			r = sip_header_via(value, end, &msg->vias);
		}
		else if (0 == strcasecmp("Contact", name))
		{
			r = sip_header_contacts(value, end, &msg->contacts);
		}
		else if (0 == strcasecmp("Route", name))
		{
			r = sip_header_uri(value, end, &uri);
			if(0 == r)
				sip_uris_push(&msg->routers, &uri);
		}
		else if (0 == strcasecmp("Record-Route", name))
		{
			r = sip_header_uri(value, end, &uri);
			if (0 == r)
				sip_uris_push(&msg->record_routers, &uri);
		}
		else
		{
			// ignore
		}
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
	if (p < end) p += snprintf(p, end - p, "\r\nTo: ");
	if (p < end) p += sip_contact_write(&msg->to, p, end);
	if (p < end) p += snprintf(p, end - p, "\r\nFrom: ");
	if (p < end) p += sip_contact_write(&msg->to, p, end);
	if (p < end) p += snprintf(p, end - p, "\r\nCall-ID: ");
	if (p < end) p += cstrcpy(&msg->callid, p, end - p);
	if (p < end) p += snprintf(p, end - p, "\r\nCSeq: ");
	if (p < end) p += sip_cseq_write(&msg->cseq, p, end);
	if (p < end) p += snprintf(p, end - p, "\r\nMax-Forwards: %u", msg->maxforwards);
	for (i = 0; i < sip_vias_count((struct sip_vias_t*)&msg->vias); i++)
	{
		if (p < end) p += snprintf(p, end - p, "\r\nVia: ");
		if (p < end) p += sip_via_write(sip_vias_get((struct sip_vias_t*)&msg->vias, i), p, end);
	}

	// contacts
	for (i = 0; i < sip_contacts_count((struct sip_contacts_t*)&msg->contacts); i++)
	{
		if (p < end) p += snprintf(p, end - p, "\r\nContact: ");
		if (p < end) p += sip_contact_write(sip_contacts_get((struct sip_contacts_t*)&msg->contacts, i), p, end);
	}

	// routers
	for (i = 0; i < sip_uris_count((struct sip_uris_t*)&msg->routers); i++)
	{
		if (p < end) p += snprintf(p, end - p, "\r\nRoute: ");
		if (p < end) p += sip_uri_write(sip_uris_get((struct sip_uris_t*)&msg->routers, i), p, end);
	}

	// record-routers
	for (i = 0; i < sip_uris_count((struct sip_uris_t*)&msg->record_routers); i++)
	{
		if (p < end) p += snprintf(p, end - p, "\r\nRecord-Route: ");
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
