#include "sip-uac.h"
#include "sip-dialog.h"
#include "sip-message.h"
#include "sip-uac-transaction.h"

static int sip_uac_onbye(struct sip_uac_transaction_t* t, const struct sip_message_t* reply)
{
	struct sip_dialog_t* dialog;
	if ( (200 <= reply->u.s.code && reply->u.s.code < 300) || 481 == reply->u.s.code )
	{
		dialog = sip_dialog_fetch(t->agent, &reply->callid, &reply->from.tag, &reply->to.tag);
		if (dialog)
		{
			sip_dialog_remove(t->agent, dialog);
			sip_dialog_release(dialog);
		}
	}
	return 0;
}

struct sip_uac_transaction_t* sip_uac_bye(struct sip_agent_t* sip, struct sip_dialog_t* dialog, sip_uac_onreply onbye, void* param)
{
	struct sip_message_t* req;
	struct sip_uac_transaction_t* t;

	++dialog->local.id;
	req = sip_message_create(SIP_MESSAGE_REQUEST);
	if (0 != sip_message_init2(req, SIP_METHOD_BYE, dialog))
	{
		--dialog->local.id;
		sip_message_destroy(req);
		return NULL;
	}

	t = sip_uac_transaction_create(sip, req);
	t->onhandle = sip_uac_onbye;
	t->onreply = onbye;
	t->param = param;
	return t;
}
