#include "sip-agent.h"
#include "sip-internal.h"
//#include "sip-uac-transaction.h"
//#include "sip-uas-transaction.h"

struct sip_agent_t* sip_agent_create(struct sip_uas_handler_t* handler, void* param)
{
	struct sip_agent_t* sip;
	sip = (struct sip_agent_t*)calloc(1, sizeof(*sip));
	if (NULL == sip)
		return NULL;

	sip->ref = 1;
	locker_create(&sip->locker);
	LIST_INIT_HEAD(&sip->uac);
	LIST_INIT_HEAD(&sip->uas);
	LIST_INIT_HEAD(&sip->dialogs);
	LIST_INIT_HEAD(&sip->subscribes);
	memcpy(&sip->handler, handler, sizeof(sip->handler));
	sip->param = param;
	return sip;
}

int sip_agent_destroy(struct sip_agent_t* sip)
{
    int32_t ref;
	//struct list_head *pos, *next;
	//struct sip_dialog_t* dialog;
	//struct sip_subscribe_t* subscribe;
	//struct sip_uac_transaction_t* uac;
	//struct sip_uas_transaction_t* uas;

	assert(sip->ref > 0);
    ref = atomic_decrement32(&sip->ref);
	if (0 != ref)
		return ref;

	assert(list_empty(&sip->uac));
	assert(list_empty(&sip->uas));
	assert(list_empty(&sip->dialogs));
	assert(list_empty(&sip->subscribes));
	//list_for_each_safe(pos, next, &sip->uac)
	//{
	//	uac = list_entry(pos, struct sip_uac_transaction_t, link);
	//	assert(uac->agent == sip);
	//	sip_uac_transaction_release(uac);
	//}
	//list_for_each_safe(pos, next, &sip->uas)
	//{
	//	uas = list_entry(pos, struct sip_uas_transaction_t, link);
	//	assert(uas->agent == sip);
	//	sip_uac_transaction_release(uas);
	//}
	//list_for_each_safe(pos, next, &sip->dialogs)
	//{
	//	dialog = list_entry(pos, struct sip_dialog_t, link);
	//	sip_dialog_release(dialog);
	//}
	//list_for_each_safe(pos, next, &sip->subscribes)
	//{
	//	subscribe = list_entry(pos, struct sip_subscribe_t, link);
	//	sip_subscribe_release(subscribe);
	//}

	locker_destroy(&sip->locker);
	free(sip);
	return 0;
}

int sip_agent_input(struct sip_agent_t* sip, struct sip_message_t* msg)
{
	return SIP_MESSAGE_REPLY == msg->mode ? sip_uac_input(sip, msg) : sip_uas_input(sip, msg);
}
