#include "sip-uas-transaction.h"

int sip_uas_onrefer(struct sip_uas_transaction_t* t, struct sip_dialog_t* dialog, const struct sip_message_t* req)
{
	int r;
	// An agent responding to a REFER method MUST return a 400 (Bad Request)
	// if the request contained zero or more than one Refer-To header field values.
	if (!cstrvalid(&req->referto.uri.host))
		return sip_uas_reply(t, 400, NULL, 0);

	if(t->handler->onrefer)
		r = t->handler->onrefer(t->param, req, t, dialog ? dialog->session : NULL);
	else
		r = 0; // just ignore
	return r;
}
