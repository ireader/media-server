#include "sip-uac.h"
#include "sip-dialog.h"
#include "sip-message.h"
#include "sip-uac-transaction.h"

struct sip_uac_transaction_t* sip_uac_custom(struct sip_uac_t* uac, const char* method, const char* from, const char* to, sip_uac_onreply onoptins, void* param)
{
	struct sip_message_t* req;
	struct sip_uac_transaction_t* t;

	req = sip_message_create(SIP_MESSAGE_REQUEST);
	if (0 != sip_message_init(req, method, to, from, to))
	{
		sip_message_destroy(req);
		return NULL;
	}

	t = sip_uac_transaction_create(uac, req);
	t->onreply = onoptins;
	t->param = param;
	return t;
}

struct sip_uac_transaction_t* sip_uac_options(struct sip_uac_t* uac, const char* name, const char* to, sip_uac_onreply onoptins, void* param)
{
	return sip_uac_custom(uac, SIP_METHOD_OPTIONS, name, to, onoptins, param);
}

struct sip_uac_transaction_t* sip_uac_info(struct sip_uac_t* uac, const char* from, const char* to, sip_uac_onreply oninfo, void* param)
{
	return sip_uac_custom(uac, SIP_METHOD_INFO, from, to, oninfo, param);
}

struct sip_uac_transaction_t* sip_uac_message(struct sip_uac_t* uac, const char* from, const char* to, sip_uac_onreply onmsg, void* param)
{
	return sip_uac_custom(uac, SIP_METHOD_MESSAGE, from, to, onmsg, param);
}

struct sip_uac_transaction_t* sip_uac_subscribe(struct sip_uac_t* uac, const char* from, const char* to, sip_uac_onreply onsubscribe, void* param)
{
	return sip_uac_custom(uac, SIP_METHOD_SUBSCRIBE, from, to, onsubscribe, param);
}

struct sip_uac_transaction_t* sip_uac_notify(struct sip_uac_t* uac, const char* from, const char* to, sip_uac_onreply onnotify, void* param)
{
	return sip_uac_custom(uac, SIP_METHOD_NOTIFY, from, to, onnotify, param);
}
