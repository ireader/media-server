#include "sip-uac.h"
#include "sip-dialog.h"
#include "sip-message.h"
#include "sip-uac-transaction.h"

struct sip_uac_transaction_t* sip_uac_custom(struct sip_agent_t* sip, const char* method, const char* from, const char* to, sip_uac_onreply onrely, void* param)
{
	struct sip_message_t* req;
	struct sip_uac_transaction_t* t;

	req = sip_message_create(SIP_MESSAGE_REQUEST);
    if (0 != sip_message_init(req, method, to, from, to))
	{
        sip_message_destroy(req);
		return NULL;
	}

	t = sip_uac_transaction_create(sip, req);
	t->onreply = onrely;
	t->param = param;
	return t;
}

struct sip_uac_transaction_t* sip_uac_options(struct sip_agent_t* sip, const char* name, const char* to, sip_uac_onreply onoptions, void* param)
{
	return sip_uac_custom(sip, SIP_METHOD_OPTIONS, name, to, onoptions, param);
}

struct sip_uac_transaction_t* sip_uac_message(struct sip_agent_t* sip, const char* from, const char* to, sip_uac_onreply onmsg, void* param)
{
	return sip_uac_custom(sip, SIP_METHOD_MESSAGE, from, to, onmsg, param);
}

struct sip_uac_transaction_t* sip_uac_refer(struct sip_agent_t* sip, const char* from, const char* to, sip_uac_onreply onreply, void* param)
{
	return sip_uac_custom(sip, SIP_METHOD_REFER, from, to, onreply, param);
}