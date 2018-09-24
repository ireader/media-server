#include "sip-uac.h"
#include "sip-dialog.h"
#include "sip-message.h"
#include "sip-uac-transaction.h"

struct sip_uac_transaction_t* sip_uac_options(struct sip_uac_t* uac, const char* name, const char* to, sip_uac_onreply onoptins, void* param)
{
	struct sip_message_t* req;
	struct sip_uac_transaction_t* t;

	req = sip_message_create(SIP_MESSAGE_REQUEST);
	if (0 != sip_message_init(req, SIP_METHOD_OPTIONS, to, name, to))
	{
		sip_message_destroy(req);
		return NULL;
	}

	t = sip_uac_transaction_create(uac, req);
	t->onreply = onoptins;
	t->param = param;
	return t;
}
