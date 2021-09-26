#include "sip-uac.h"
#include "sip-dialog.h"
#include "sip-message.h"
#include "sip-uac-transaction.h"

struct sip_uac_transaction_t* sip_uac_dialog_custom(struct sip_agent_t* sip, const char* method, struct sip_dialog_t* dialog, sip_uac_onreply onreply, void* param)
{
    struct sip_message_t* req;
    struct sip_uac_transaction_t* t;

    if (!dialog || !method)
        return NULL;

    ++dialog->local.id;
    req = sip_message_create(SIP_MESSAGE_REQUEST);
    if(0 != sip_message_init2(req, method, dialog))
    {
        --dialog->local.id;
        sip_message_destroy(req);
        return NULL;
    }

    t = sip_uac_transaction_create(sip, req);
    t->onreply = onreply;
    t->param = param;
    return t;
}

struct sip_uac_transaction_t* sip_uac_info(struct sip_agent_t* sip, struct sip_dialog_t* dialog, const char* package, sip_uac_onreply oninfo, void* param)
{
    // INFO messages cannot be sent as part of other dialog usages, or outside an existing dialog.

	struct sip_uac_transaction_t* t;
    t = sip_uac_dialog_custom(sip, SIP_METHOD_INFO, dialog, oninfo, param);
    if(!t)
        return NULL;
    
    if(0 != sip_uac_add_header(t, SIP_HEADER_INFO_PACKAGE, package))
	{
		--dialog->local.id; // dec Dialog CSeq
		sip_uac_transaction_release(t);
		return NULL;
	}

	return t;
}
