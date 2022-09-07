#include "sip-uas-transaction.h"
#include "sip-subscribe.h"

int sip_uas_onsubscribe(struct sip_uas_transaction_t* t, struct sip_dialog_t* dialog, const struct sip_message_t* req, void* param)
{
	int r, added;
	const struct cstring_t *h;
	struct sip_subscribe_t* subscribe;

	r = 0;
	subscribe = sip_subscribe_internal_fetch(t->agent, req, &req->event, 0, &added);
	if (!subscribe)
		return sip_uas_reply(t, 481, NULL, 0, param); // 481 Subscription does not exist

	assert(!dialog || subscribe->dialog == dialog);
	if (subscribe->dialog)
	{
		sip_dialog_setlocaltag(subscribe->dialog, &t->reply->to.tag);
		sip_dialog_set_local_target(subscribe->dialog, req);
	}

	h = sip_message_get_header_by_name(req, "Expires");
	subscribe->expires = h ? (uint64_t)cstrtoll(h, NULL, 10) : 0;

	// call once only
	if ( /*added &&*/ t->handler->onsubscribe)
		r = t->handler->onsubscribe(param, req, t, subscribe, &subscribe->evtsession);

	if (subscribe)
	{
		// delete subscribe if expires is 0
		if (h && 0 == subscribe->expires)
		{
            // notify expire
            //if (t->handler->onnotify)
            //    t->handler->onnotify(param, req, t, subscribe->evtsession, NULL);
			sip_subscribe_remove(t->agent, subscribe);
			assert(1 == subscribe->ref);
		}

		sip_subscribe_release(subscribe);
	}

	return r;

	// TODO:
	// Note that a NOTIFY message is always sent immediately after any 200-
	// class response to a SUBSCRIBE request, regardless of whether the
	// subscription has already been authorized.

	// If subscription authorization was delayed and the notifier wishes to
	// convey that such authorization has been declined, it may do so by
	// sending a NOTIFY message containing a "Subscription-State" header
	// with a value of "terminated" and a reason parameter of "rejected".

	// When removing a subscription, the notifier SHOULD send a NOTIFY message 
	// with a "Subscription-State" value of "terminated" to inform it that the
	// subscription is being removed. If such a message is sent, the 
	// "Subscription-State" header SHOULD contain a "reason=timeout" parameter.
}

int sip_uas_onnotify(struct sip_uas_transaction_t* t, const struct sip_message_t* req, void* param)
{
	int r;
	struct sip_subscribe_t* subscribe;

	subscribe = sip_subscribe_fetch(t->agent, &req->callid, &req->to.tag, &req->from.tag, &req->event);
	//if (!subscribe)
	//	return sip_uas_reply(t, 481, NULL, 0, param); // 481 Subscription does not exist

	// 489 Bad Event
	if (t->handler->onnotify)
		r = t->handler->onnotify(param, req, t, subscribe ? subscribe->evtsession : NULL, &req->event);
	else
		r = 0; // just ignore

	if (subscribe && 0 == cstrcmp(&req->substate.state, SIP_SUBSCRIPTION_STATE_TERMINATED))
		sip_subscribe_remove(t->agent, subscribe);

	sip_subscribe_release(subscribe);
	return r;
}

int sip_uas_onpublish(struct sip_uas_transaction_t* t, const struct sip_message_t* req, void* param)
{
	int r;
	if (t->handler->onpublish)
		r = t->handler->onpublish(param, req, t, &req->event);
	else
		r = 0; // just ignore
	return r;
}
