#include "sip-uac.h"
#include "sip-dialog.h"
#include "sip-message.h"
#include "sip-uac-transaction.h"

struct sip_uac_transaction_t* sip_uac_bye(struct sip_uac_t* uac, struct sip_dialog_t* dialog, sip_uac_onreply onbye, void* param)
{
	struct sip_message_t* req;
	struct sip_uac_transaction_t* t;

	++dialog->local.id;
	req = sip_message_create();
	if (0 != sip_message_init2(req, SIP_METHOD_BYE, dialog))
	{
		--dialog->local.id;
		sip_message_destroy(req);
		return NULL;
	}

	t = sip_uac_transaction_create(uac, req);
	t->onreply = onbye;
	t->param = param;
	return t;
}
