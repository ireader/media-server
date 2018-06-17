#include "sip-message.h"
#include <stdio.h>

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
