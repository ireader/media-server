#include "sip-uas-transaction.h"

int sip_uas_oninfo(struct sip_uas_transaction_t* t, struct sip_dialog_t* dialog, const struct sip_message_t* req)
{
	int r;
	assert(dialog);

	if(!cstrvalid(&req->info_package))
		r = sip_uas_reply(t, 469, NULL, 0); // 469 Bad Info Package

	// TODO: check Info-Package

	if(t->handler->oninfo)
		r = t->handler->oninfo(t->param, req, t, dialog ? dialog->session : NULL);
	else
		r = 0; // just ignore
	return r;
}
