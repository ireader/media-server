#include "sip-uas-transaction.h"

int sip_uas_onprack(struct sip_uas_transaction_t* t, struct sip_dialog_t* dialog, const struct sip_message_t* req, void* param)
{
	int r;
	if (!dialog)
	{
		assert(0);
		return 0; // discard
	}

	if (t->handler->onprack)
		r = t->handler->onprack(param, req, t, dialog ? dialog->session : NULL, dialog, req->payload, req->size);
	else
		r = 0; // just ignore
	return r;
}

int sip_uas_onupdate(struct sip_uas_transaction_t* t, struct sip_dialog_t* dialog, const struct sip_message_t* req, void* param)
{
	int r;
	if (!dialog)
	{
		assert(0);
		return 0; // discard
	}

	if (t->handler->onupdate)
		r = t->handler->onupdate(param, req, t, dialog ? dialog->session : NULL, dialog, req->payload, req->size);
	else
		r = 0; // just ignore
	return r;
}
