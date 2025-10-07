#include "sip-agent.h"
#include "sip-internal.h"
//#include "sip-uac-transaction.h"
//#include "sip-uas-transaction.h"

struct sip_agent_t* sip_agent_create(struct sip_uas_handler_t* handler)
{
	struct sip_agent_t* sip;
	sip = (struct sip_agent_t*)calloc(1, sizeof(*sip));
	if (NULL == sip)
		return NULL;

	sip->ref = 1;
	locker_create(&sip->locker);
	LIST_INIT_HEAD(&sip->uac);
	LIST_INIT_HEAD(&sip->uas);
	memcpy(&sip->handler, handler, sizeof(sip->handler));
	return sip;
}

int sip_agent_destroy(struct sip_agent_t* sip)
{
    int32_t ref;

	assert(sip->ref > 0);
    ref = atomic_decrement32(&sip->ref);
	if (0 != ref)
		return ref;

	assert(list_empty(&sip->uac));
	assert(list_empty(&sip->uas));
	
	locker_destroy(&sip->locker);
	free(sip);
	return 0;
}

int sip_agent_input(struct sip_agent_t* sip, struct sip_message_t* msg, void* param)
{
	return SIP_MESSAGE_REPLY == msg->mode ? sip_uac_input(sip, msg) : sip_uas_input(sip, msg, param);
}

int sip_agent_set_rport(struct sip_message_t* msg, const char* peer, int port)
{
	return sip_message_set_rport(msg, peer, port);
}

struct sip_gc_t s_gc;
void sip_gc_get(int32_t* uac, int32_t* uas, int32_t* dialog, int32_t* message, int32_t* subscribe)
{
	*uac = s_gc.uac;
	*uas = s_gc.uas;
	*dialog = s_gc.dialog;
	*message = s_gc.message;
	*subscribe = s_gc.subscribe;
}
