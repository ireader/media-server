#include "sip-header.h"
#include <stdio.h>

/*
Subscription-State = "Subscription-State" HCOLON substate-value *( SEMI subexp-params )
substate-value = "active" / "pending" / "terminated" / extension-substate
extension-substate = token
subexp-params = ("reason" EQUAL event-reason-value) / ("expires" EQUAL delta-seconds) / ("retry-after" EQUAL delta-seconds) / generic-param
event-reason-value = "deactivated" / "probation" / "rejected" / "timeout" / "giveup" / "noresource" / "invariant"/ event-reason-extension
event-reason-extension = token
*/
int sip_header_substate(const char* s, const char* end, struct sip_substate_t* substate)
{
	int r, i;
	const char* p;
	const struct sip_param_t* param;
	memset(substate, 0, sizeof(*substate));
	sip_params_init(&substate->params);

	sscanf(s, " %n%*[^ ;\t\r\n]%n", &i, &r);
	substate->state.p = s + i;
	substate->state.n = r - i;

	// params
	r = 0;
	p = substate->state.p + substate->state.n;
	if (p && p < end && ';' == *p)
	{
		r = sip_header_params(';', p + 1, end, &substate->params);

		for (i = 0; i < sip_params_count(&substate->params); i++)
		{
			param = sip_params_get(&substate->params, i);

			if (0 == cstrcmp(&param->name, "reason"))
			{
				substate->reason.p = param->value.p;
				substate->reason.n = param->value.n;
			}
			else if (0 == cstrcmp(&param->name, "expires"))
			{
				substate->expires = atoi(param->value.p);
			}
			else if (0 == cstrcmp(&param->name, "retry-after"))
			{
				substate->retry = atoi(param->value.p);
			}
		}
	}
	return 0;
}

int sip_substate_write(const struct sip_substate_t* substate, char* data, const char* end)
{
	int n;
	char *p;
	if (!cstrvalid(&substate->state))
		return -1;

	p = data;
	if (p < end)
		p += snprintf(p, end - p, "%.*s", (int)substate->state.n, substate->state.p);

	if (sip_params_count(&substate->params) > 0)
	{
		if (p < end) *p++ = ';';
		n = sip_params_write(&substate->params, p, end, ';');
		if (n < 0) return n;
		p += n;
	}

	if (p < end) *p = '\0';
	return (int)(p - data);
}

#if defined(DEBUG) || defined(_DEBUG)
void sip_header_substate_test(void)
{
	char buf[64];
	const char* s;
	struct sip_substate_t substate;

	s = "active;expires=600000";
	assert(0 == sip_header_substate(s, s + strlen(s), &substate));
	assert(0 == cstrcmp(&substate.state, "active") && substate.expires == 600000);
	assert(strlen(s) == sip_substate_write(&substate, buf, buf + sizeof(buf)));
	assert(0 == strcmp(s, buf));
}
#endif
