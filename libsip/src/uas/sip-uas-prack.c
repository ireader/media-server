#include "sip-uas-transaction.h"

int sip_uas_onprack(struct sip_uas_transaction_t* t, const struct sip_message_t* req, void* param)
{
	int r;
	char ptr[256];
	struct cstring_t id;

	sip_dialog_id_with_message(&id, req, ptr, sizeof(ptr), 1);
	if (t->handler->onprack)
		r = t->handler->onprack(param, req, t, &id, req->payload, req->size);
	else
		r = 0; // just ignore
	return r;
}

int sip_uas_onupdate(struct sip_uas_transaction_t* t, const struct sip_message_t* req, void* param)
{
	int r;
	char ptr[256];
	struct cstring_t id;

	sip_dialog_id_with_message(&id, req, ptr, sizeof(ptr), 1);
	if (t->handler->onupdate)
		r = t->handler->onupdate(param, req, t, &id, req->payload, req->size);
	else
		r = 0; // just ignore
	return r;
}
