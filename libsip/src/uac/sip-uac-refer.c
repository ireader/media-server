#include "sip-uac.h"
#include "sip-dialog.h"
#include "sip-message.h"
#include "sip-uac-transaction.h"
#include <stdio.h>

struct sip_uac_transaction_t* sip_uac_refer(struct sip_agent_t* sip, const char* from, const char* to, sip_uac_onreply onreply, void* param)
{
	struct sip_message_t* req;
	struct sip_uac_transaction_t* t;

	if (!sip)
		return NULL;

	req = sip_message_create(SIP_MESSAGE_REQUEST);
	if (0 != sip_message_init(req, SIP_METHOD_REFER, to, from, to))
	{
		sip_message_destroy(req);
		return NULL;
	}

	t = sip_uac_transaction_create(sip, req);
	t->onreply = onreply;
	t->param = param;
	return t;
}
