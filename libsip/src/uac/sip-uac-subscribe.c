#include "sip-uac.h"
#include "sip-dialog.h"
#include "sip-message.h"
#include "sip-subscribe.h"
#include "sip-uac-transaction.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int sip_uac_subscribe_onreply(struct sip_uac_transaction_t* t, const struct sip_message_t* reply)
{
	int r;
	int added;
	const struct cstring_t *h;
	struct sip_subscribe_t* subscribe;

	if (reply->u.s.code < 200)
		return 0; // ignore

	r = 0;
	subscribe = NULL;
	if (200 <= reply->u.s.code && reply->u.s.code < 300)
	{
		subscribe = sip_subscribe_internal_fetch(t->agent, reply, &t->req->event, 1, &added);

		// call once only
		//if (added)
			r = t->onsubscribe(t->param, reply, t, subscribe, reply->u.s.code, &subscribe->evtsession);
	}

	if (subscribe)
	{
		// delete subscribe if expires is 0
		h = sip_message_get_header_by_name(t->req, "Expires");
		if (h && 0 == cstrtol(h, NULL, 10))
		{
			sip_subscribe_remove(t->agent, subscribe);
			assert(1 == subscribe->ref);
		}

		sip_subscribe_release(subscribe);
	}

	return r;
}

int sip_uac_notify_onreply(struct sip_uac_transaction_t* t, const struct sip_message_t* reply)
{
	int r;
	struct sip_subscribe_t* subscribe;

	subscribe = sip_subscribe_fetch(t->agent, &reply->callid, &reply->from.tag, &reply->to.tag, &t->req->event);
	if (!subscribe)
		return 0; // receive notify message before subscribe reply, discard it

	// NOTICE: ignore notify before subscribe created
	r = t->onreply(t->param, reply, t, 200 <= reply->u.s.code);

	if (0 == cstrcmp(&reply->substate.state, SIP_SUBSCRIPTION_STATE_TERMINATED))
		sip_subscribe_remove(t->agent, subscribe);

	sip_subscribe_release(subscribe);
	return r;
}

struct sip_uac_transaction_t* sip_uac_subscribe(struct sip_agent_t* sip, const char* from, const char* to, const char* event, int expires, sip_uac_onsubscribe onsubscribe, void* param)
{
	struct sip_message_t* req;
	struct sip_uac_transaction_t* t;

	req = sip_message_create(SIP_MESSAGE_REQUEST);
	if (0 != sip_message_init(req, SIP_METHOD_SUBSCRIBE, to, from, to)
		|| 0 != sip_message_add_header(req, SIP_HEADER_EVENT, event)
		|| 0 != sip_message_add_header_int(req, "Expires", expires))
	{
		sip_message_destroy(req);
		return NULL;
	}

	t = sip_uac_transaction_create(sip, req);
	t->onsubscribe = onsubscribe;
	t->param = param;
	return t;
}

struct sip_uac_transaction_t* sip_uac_resubscribe(struct sip_agent_t* sip, struct sip_subscribe_t* subscribe, int expires, sip_uac_onsubscribe onsubscribe, void* param)
{
	struct sip_message_t* req;
	struct sip_uac_transaction_t* t;

	if (!sip || !subscribe || !subscribe->dialog)
		return NULL;

	++subscribe->dialog->local.id;
	req = sip_message_create(SIP_MESSAGE_REQUEST);
	if (0 != sip_message_init2(req, SIP_METHOD_SUBSCRIBE, subscribe->dialog)
		|| 0 != sip_message_add_header(req, SIP_HEADER_EVENT, subscribe->event)
		|| 0 != sip_message_add_header_int(req, "Expires", expires))
	{
		sip_message_destroy(req);
		return NULL;
	}

	t = sip_uac_transaction_create(sip, req);
	t->onsubscribe = onsubscribe;
	t->param = param;
	return t;
}

struct sip_uac_transaction_t* sip_uac_notify(struct sip_agent_t* sip, struct sip_subscribe_t* subscribe, const char* state, sip_uac_onreply onnotify, void* param)
{
	struct sip_message_t* req;
	struct sip_uac_transaction_t* t;

	if (!subscribe || !subscribe->dialog)
		return NULL;

	++subscribe->dialog->local.id;
	req = sip_message_create(SIP_MESSAGE_REQUEST);
	if(0 != sip_message_init2(req, SIP_METHOD_NOTIFY, subscribe->dialog)
		|| 0 != sip_message_add_header(req, SIP_HEADER_EVENT, subscribe->event)
		|| 0 != sip_message_add_header(req, SIP_HEADER_SUBSCRIBE_STATE, state))
	{
		--subscribe->dialog->local.id;
		sip_message_destroy(req);
		return NULL;
	}

	// NOTICE: ignore notify before subscribe created
	t = sip_uac_transaction_create(sip, req);
	t->onreply = onnotify;
	t->param = param;
	return t;
}

struct sip_uac_transaction_t* sip_uac_publish(struct sip_agent_t* sip, const char* from, const char* to, const char* event, sip_uac_onreply onreply, void* param)
{
	struct sip_message_t* req;
	struct sip_uac_transaction_t* t;

	req = sip_message_create(SIP_MESSAGE_REQUEST);
	if (0 != sip_message_init(req, SIP_METHOD_PUBLISH, to, from, to)
		|| 0 != sip_message_add_header(req, SIP_HEADER_EVENT, event))
	{
		sip_message_destroy(req);
		return NULL;
	}

	t = sip_uac_transaction_create(sip, req);
	t->onreply = onreply;
	t->param = param;
	return t;
}
