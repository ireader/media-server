// https://www.rfc-editor.org/rfc/rfc6665#section-8.2.1

#include "sip-header.h"
#include <stdio.h>

void sip_event_free(struct sip_event_t* evt)
{
	sip_params_free(&evt->params);
}

/*
Event             =  ( "Event" / "o" ) HCOLON event-type
					*( SEMI event-param )
event-type        =  event-package *( "." event-template )
event-package     =  token-nodot
event-template    =  token-nodot
token-nodot       =  1*( alphanum / "-"  / "!" / "%" / "*"
						/ "_" / "+" / "`" / "'" / "~" )
; The use of the "id" parameter is deprecated; it is included
; for backwards-compatibility purposes only.
event-param       =  generic-param / ( "id" EQUAL token )
*/
int sip_header_event(const char* s, const char* end, struct sip_event_t* evt)
{
	int r, i;
	const char* p;
	const struct sip_param_t* param;
	memset(evt, 0, sizeof(*evt));
	sip_params_init(&evt->params);

	r = i = 0;
	if (sscanf((s && s < end) ? s : "", " %n%*[^ ;\t\r\n]%n", &i, &r) < 0 || r < i)
		return -1;
	evt->event.p = s + i;
	evt->event.n = r - i;

	// params
	r = 0;
	p = evt->event.p + evt->event.n;
	if (p && p < end && ';' == *p)
	{
		r = sip_header_params(';', p + 1, end, &evt->params);

		for (i = 0; i < sip_params_count(&evt->params); i++)
		{
			param = sip_params_get(&evt->params, i);

			if (0 == cstrcmp(&param->name, "id"))
			{
				evt->id.p = param->value.p;
				evt->id.n = param->value.n;
			}
		}
	}
	return 0;
}

int sip_event_write(const struct sip_event_t* evt, char* data, const char* end)
{
	int n;
	char* p;
	if (!cstrvalid(&evt->event))
		return -1;

	p = data;
	if (p < end)
		p += snprintf(p, end - p, "%.*s", (int)evt->event.n, evt->event.p);

	if (sip_params_count(&evt->params) > 0)
	{
		if (p < end) *p++ = ';';
		n = sip_params_write(&evt->params, p, end, ';');
		if (n < 0) return n;
		p += n;
	}

	if (p < end) *p = '\0';
	return (int)(p - data);
}

int sip_event_equal(const struct sip_event_t* l, const struct sip_event_t* r)
{
	// For the purposes of matching NOTIFY requests with SUBSCRIBE requests,
	// the event-type portion of the "Event" header field is compared byte
	// by byte, and the "id" parameter token (if present) is compared byte
	// by byte.  An "Event" header field containing an "id" parameter never
	// matches an "Event" header field without an "id" parameter.  No other
	// parameters are considered when performing a comparison.  SUBSCRIBE
	// responses are matched per the transaction handling rules in
	// [RFC3261].

	if (!cstreq(&l->event, &r->event))
		return 0;

	if (l->id.n != r->id.n || !cstreq(&l->id, &r->id))
		return 0;

	return 1;
}

#if defined(DEBUG) || defined(_DEBUG)
void sip_header_event_test(void)
{
	char buf[64];
	const char* s, *s2;
	struct sip_event_t evt, evt2;

	s = "foo;id=1234";
	assert(0 == sip_header_event(s, s + strlen(s), &evt));
	assert(0 == cstrcmp(&evt.event, "foo") && 0 == cstrcmp(&evt.id, "1234"));
	assert((int)strlen(s) == sip_event_write(&evt, buf, buf + sizeof(buf)));
	assert(0 == strcmp(s, buf));
	
	s2 = " foo; param=abcd; id=1234";
	assert(0 == sip_header_event(s2, s2 + strlen(s2), &evt2) && sip_event_equal(&evt, &evt2));
	sip_event_free(&evt2);

	s2 = " foo";
	assert(0 == sip_header_event(s2, s2 + strlen(s2), &evt2) && !sip_event_equal(&evt, &evt2) && !sip_event_equal(&evt2, &evt));
	sip_event_free(&evt2);

	s2 = " foo; id=123";
	assert(0 == sip_header_event(s2, s2 + strlen(s2), &evt2) && !sip_event_equal(&evt, &evt2));
	sip_event_free(&evt2);

	s2 = " abc; id=1234";
	assert(0 == sip_header_event(s2, s2 + strlen(s2), &evt2) && !sip_event_equal(&evt, &evt2));
	sip_event_free(&evt2);

	sip_event_free(&evt);
}
#endif
