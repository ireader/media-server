#include "sip-uac.h"
#include "sip-dialog.h"
#include "sip-message.h"
#include "sip-uac-transaction.h"

struct sip_uac_transaction_t* sip_uac_cancel(struct sip_uac_t* uac, struct sip_dialog_t* dialog, sip_uac_onreply oncancel, void* param)
{
	struct sip_message_t* req;
	struct sip_uac_transaction_t* t;

	++dialog->local.id;
	req = sip_message_create(SIP_MESSAGE_REQUEST);
	if (0 != sip_message_init2(req, SIP_METHOD_CANCEL, dialog))
	{
		--dialog->local.id;
		sip_message_destroy(req);
		return NULL;
	}

	t = sip_uac_transaction_create(uac, req);
	t->onreply = oncancel;
	t->param = param;
	return t;
}
