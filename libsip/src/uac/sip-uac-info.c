#include "sip-uac.h"
#include "sip-dialog.h"
#include "sip-message.h"
#include "sip-uac-transaction.h"

struct sip_uac_transaction_t* sip_uac_info(struct sip_agent_t* sip, struct sip_dialog_t* dialog, const char* package, sip_uac_onreply oninfo, void* param)
{
	struct sip_message_t* req;
	struct sip_uac_transaction_t* t;

	if (!dialog || !package)
		return NULL;

	++dialog->local.id;
	req = sip_message_create(SIP_MESSAGE_REQUEST);
	if(0 != sip_message_init2(req, SIP_METHOD_INFO, dialog)
		|| 0 != sip_message_add_header(req, SIP_HEADER_INFO_PACKAGE, package))
	{
		--dialog->local.id;
		sip_message_destroy(req);
		return NULL;
	}

	t = sip_uac_transaction_create(sip, req);
	t->onreply = oninfo;
	t->param = param;
	return t;
}
