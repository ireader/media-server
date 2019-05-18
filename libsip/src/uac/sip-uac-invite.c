#include "sip-uac.h"
#include "sip-dialog.h"
#include "sip-message.h"
#include "sip-uac-transaction.h"

struct sip_uac_transaction_t* sip_uac_invite(struct sip_agent_t* sip, const char* name, const char* to, sip_uac_oninvite oninvite, void* param)
{
	struct sip_message_t* req;
	struct sip_uac_transaction_t* t;

	req = sip_message_create(SIP_MESSAGE_REQUEST);
	if (0 != sip_message_init(req, SIP_METHOD_INVITE, to, name, to))
	{
		sip_message_destroy(req);
		return NULL;
	}

	t = sip_uac_transaction_create(sip, req);
	t->oninvite = oninvite;
	t->param = param;
	return t;
}

struct sip_uac_transaction_t* sip_uac_reinvite(struct sip_agent_t* sip, struct sip_dialog_t* dialog, sip_uac_oninvite oninvite, void* param)
{
	struct sip_message_t* req;
	struct sip_uac_transaction_t* t;

	++dialog->local.id;
	req = sip_message_create(SIP_MESSAGE_REQUEST);
	if (0 != sip_message_init2(req, SIP_METHOD_INVITE, dialog))
	{
		--dialog->local.id;
		sip_message_destroy(req);
		return NULL;
	}

	t = sip_uac_transaction_create(sip, req);
	t->oninvite = oninvite;
	t->param = param;
	return t;
}
