#include "sip-uac.h"
#include "sip-dialog.h"
#include "sip-message.h"
#include "sip-uac-transaction.h"

struct sip_uac_transaction_t* sip_uac_register(struct sip_agent_t* sip, const char* name, const char* registrar, int seconds, sip_uac_onreply onregister, void* param)
{
	struct sip_message_t* req;
	struct sip_uac_transaction_t* t;

	// 1. All registrations from a UAC SHOULD use the same Call-ID header 
	//    field value for registrations sent to a particular registrar.
	// 2. The CSeq value guarantees proper ordering of REGISTER requests. 
	//	  A UA MUST increment the CSeq value by one for each REGISTER request 
	//	  with the same Call-ID.

	req = sip_message_create(SIP_MESSAGE_REQUEST);
	if (0 != sip_message_init(req, SIP_METHOD_REGISTER, registrar ? registrar : name, name, name))
	{
		sip_message_destroy(req);
		return NULL;
	}

	sip_message_add_header_int(req, "Expires", seconds);

	t = sip_uac_transaction_create(sip, req);
	t->onreply = onregister;
	t->param = param;
	return t;
}
