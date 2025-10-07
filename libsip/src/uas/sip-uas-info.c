#include "sip-uas-transaction.h"

int sip_uas_oninfo(struct sip_uas_transaction_t* t, const struct sip_message_t* req, void* param)
{
	int r;
	char ptr[256];
	struct cstring_t id;

	sip_dialog_id_with_message(&id, req, ptr, sizeof(ptr));

    // compatible rfc 2976 as "legacy INFO Usage"
//	if(!cstrvalid(&req->info_package))
//		r = sip_uas_reply(t, 469, NULL, 0); // 469 Bad Info Package

	// TODO: check Info-Package

	if(t->handler->oninfo)
		r = t->handler->oninfo(param, req, t, &id, &req->info_package, req->payload, req->size);
	else
		r = 0; // just ignore
	return r;
}
