#include "sip-uas-transaction.h"

int sip_uas_onregister(struct sip_uas_transaction_t* t, const struct sip_message_t* req)
{
	const char* expires;
	
	// All registrations from a UAC SHOULD use the same Call-ID header 
	// field value for registrations sent to a particular registrar.
	req->callid;

	// A UA MUST increment the CSeq value by one for each
	// REGISTER request with the same Call-ID.
	assert(0 == cstrcasecmp(&req->cseq.method, "REGISTER"));
	req->cseq.id;

	// zero or more values containing address bindings
	req->contacts;

	// If contact.expire is not provided, 
	// the value of the Expires header field is used instead
	expires = http_get_header_by_name(t->http, "Expires");


	t->uas->handler->onregister(t->uas->param, t, req->to);

	// The Record-Route header field has no meaning in REGISTER requests or responses, 
	// and MUST be ignored if present.
	return sip_uas_transaction_noninvite_reply(t, 200, NULL, 0);
}
