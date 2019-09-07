#include "sip-uac.h"
#include "sip-dialog.h"
#include "sip-message.h"
#include "sip-uac-transaction.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int sip_uac_oncancel(struct sip_uac_transaction_t* t, const struct sip_message_t* reply)
{
	struct sip_dialog_t* dialog;
	if (200 <= reply->u.s.code && reply->u.s.code < 300)
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

struct sip_uac_transaction_t* sip_uac_cancel(struct sip_agent_t* sip, struct sip_uac_transaction_t* invit, sip_uac_onreply oncancel, void* param)
{
	char cseq[128];
	struct sip_message_t* req;
	struct sip_uac_transaction_t* t;

	//sip_contact_write(&invit->req->from, data, end);

	// 9.1 Client Behavior (p53)
	// The Request-URI, Call-ID, To, the numeric part of CSeq, and From header
	// fields in the CANCEL request MUST be identical to those in the
	// request being cancelled, including tags.
	req = sip_message_create(SIP_MESSAGE_REQUEST);
	if(0 != sip_message_initack(req, invit->req))
	{
		sip_message_destroy(req);
		return NULL;
	}

	// overwrite REQUEST/CSEQ method value
	snprintf(cseq, sizeof(cseq), "%u %s", (unsigned int)req->cseq.id, SIP_METHOD_CANCEL);
	sip_message_add_header(req, "CSeq", cseq);
	memcpy(&req->u.c.method, &req->cseq.method, sizeof(req->u.c.method));

	t = sip_uac_transaction_create(sip, req);
	t->onhandle = sip_uac_oncancel;
	t->onreply = oncancel;
	t->param = param;
	return t;
}
