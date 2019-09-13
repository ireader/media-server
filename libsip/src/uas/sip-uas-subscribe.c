#include "sip-uas-transaction.h"
#include "sip-subscribe.h"

int sip_uas_onsubscribe(struct sip_uas_transaction_t* t, struct sip_dialog_t* dialog, const struct sip_message_t* req)
{
	int added;
	const struct cstring_t *h;
	struct sip_subscribe_t* subscribe;

	subscribe = sip_subscribe_internal_fetch(t->agent, t->reply, &req->event, 1, &added);
	if (!subscribe)
		return sip_uas_reply(t, 481, NULL, 0); // 481 Subscription does not exist

	// call once only
	if (added && t->handler->onsubscribe)
		subscribe->evtsession = t->handler->onsubscribe(t->param, req, t, subscribe);

	if (subscribe)
	{
		// delete subscribe if expires is 0
		h = sip_message_get_header_by_name(req, "Expires");
		if (h && 0 == atoi(h->p))
		{
			sip_subscribe_remove(t->agent, subscribe);
			assert(1 == subscribe->ref);
		}

		sip_subscribe_release(subscribe);
	}

	return 0;

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

int sip_uas_onnotify(struct sip_uas_transaction_t* t, const struct sip_message_t* req)
{
	int r;
	struct sip_subscribe_t* subscribe;

	subscribe = sip_subscribe_fetch(t->agent, &req->callid, &req->from.tag, &req->to.tag, &req->event);
	if (!subscribe)
		return sip_uas_reply(t, 481, NULL, 0); // 481 Subscription does not exist

	// 489 Bad Event
	if (t->handler->onnotify)
		r = t->handler->onnotify(t->param, req, t, subscribe->evtsession, &req->event);
	else
		r = 0; // just ignore

	if (0 == cstrcmp(&req->substate.state, SIP_SUBSCRIPTION_STATE_TERMINATED))
		sip_subscribe_remove(t->agent, subscribe);

	sip_subscribe_release(subscribe);
	return r;
}

int sip_uas_onpublish(struct sip_uas_transaction_t* t, const struct sip_message_t* req)
{
	int r;
	if (t->handler->onpublish)
		r = t->handler->onpublish(t->param, req, t, &req->event);
	else
		r = 0; // just ignore
	return r;
}
