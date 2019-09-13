#include "sip-uas-transaction.h"
#include "sip-transport.h"

struct sip_uas_transaction_t* sip_uas_transaction_create(struct sip_agent_t* sip, const struct sip_message_t* req)
{
	struct sip_uas_transaction_t* t;
	t = (struct sip_uas_transaction_t*)calloc(1, sizeof(*t));
	if (NULL == t) return NULL;

	t->reply = sip_message_create(SIP_MESSAGE_REPLY);
	if (0 != sip_message_init3(t->reply, req))
	{
		free(t);
		return NULL;
	}

	t->ref = 1;
	t->agent = sip;
	LIST_INIT_HEAD(&t->link);
	locker_create(&t->locker);
	t->status = SIP_UAS_TRANSACTION_INIT;

	// 17.1.1.1 Overview of INVITE Transaction (p125)
	// For unreliable transports (such as UDP), the client transaction retransmits 
	// requests at an interval that starts at T1 seconds and doubles after every retransmission.
	// 17.1.2.1 Formal Description (p130)
	// For unreliable transports, requests are retransmitted at an interval which starts at T1 and doubles until it hits T2.
	t->t2 = sip_message_isinvite(req) ? (64 * T1) : T2;

	// Life cycle: from create -> destroy
	sip_uas_add_transaction(sip, t);
	return t;
}

int sip_uas_transaction_release(struct sip_uas_transaction_t* t)
{
	assert(t->ref > 0);
	if (0 != atomic_decrement32(&t->ref))
		return 0;

	assert(0 == t->ref);
	assert(NULL == t->timerg);
	assert(NULL == t->timerh);
	assert(NULL == t->timerij);

	// MUST: destroy t->reply after sip_uas_del_transaction
	//sip_message_destroy((struct sip_message_t*)t->req);
    
    if(t->ondestroy)
    {
        t->ondestroy(t->ondestroyparam);
        t->ondestroy = NULL;
    }

	if (t->reply)
    {
		sip_message_destroy(t->reply);
        t->reply = NULL;
    }

    if(t->dialog)
    {
        sip_dialog_release(t->dialog);
        t->dialog = NULL;
    }
    
	locker_destroy(&t->locker);
	free(t);
	return 0;
}

int sip_uas_transaction_addref(struct sip_uas_transaction_t* t)
{
	assert(t->ref > 0);
	return atomic_increment32(&t->ref);
}

//int sip_uas_transaction_destroy(struct sip_uas_transaction_t* t)
//{
//	// unlink from uas
//	sip_uas_del_transaction(t->uas, t);
//
//	return sip_uas_transaction_release(t);
//}

int sip_uas_transaction_handler(struct sip_uas_transaction_t* t, struct sip_dialog_t* dialog, const struct sip_message_t* req)
{
	if (0 == cstrcasecmp(&req->u.c.method, SIP_METHOD_CANCEL))
	{
		return sip_uas_oncancel(t, dialog, req);
	}
	else if (0 == cstrcasecmp(&req->u.c.method, SIP_METHOD_BYE))
	{
		return sip_uas_onbye(t, dialog, req);
	}
	else if (0 == cstrcasecmp(&req->u.c.method, SIP_METHOD_PRACK))
	{
		return sip_uas_onprack(t, dialog, req);
	}
	else if (0 == cstrcasecmp(&req->u.c.method, SIP_METHOD_UPDATE))
	{
		return sip_uas_onupdate(t, dialog, req);
	}
	else if (0 == cstrcasecmp(&req->u.c.method, SIP_METHOD_REGISTER))
	{
		return sip_uas_onregister(t, req);
	}
	else if (0 == cstrcasecmp(&req->u.c.method, SIP_METHOD_OPTIONS))
	{
		return sip_uas_onoptions(t, req);
	}
	else if (0 == cstrcasecmp(&req->u.c.method, SIP_METHOD_SUBSCRIBE))
	{
		return sip_uas_onsubscribe(t, dialog, req);
	}
	else if (0 == cstrcasecmp(&req->u.c.method, SIP_METHOD_NOTIFY))
	{
		return sip_uas_onnotify(t, req);
	}
	else if (0 == cstrcasecmp(&req->u.c.method, SIP_METHOD_PUBLISH))
	{
		return sip_uas_onpublish(t, req);
	}
	else if (0 == cstrcasecmp(&req->u.c.method, SIP_METHOD_MESSAGE))
	{
		return t->handler->onmessage ? t->handler->onmessage(t->param, req, t, dialog, req->payload, req->size) : 0;
	}
	else if (0 == cstrcasecmp(&req->u.c.method, SIP_METHOD_INFO))
    {
        return sip_uas_oninfo(t, dialog, req);
    }
	else if (0 == cstrcasecmp(&req->u.c.method, SIP_METHOD_REFER))
	{
		return sip_uas_onrefer(t, dialog, req);
	}
	else
	{
		assert(0);
		// 8.2.1 Method Inspection (p46)
		return sip_uas_reply(t, 405/*Method Not Allowed*/, NULL, 0);
	}
}

int sip_uas_transaction_dosend(struct sip_uas_transaction_t* t)
{
	const struct sip_via_t *via;

	assert(t->size > 0);

	// 18.2.2 Sending Responses (p146)
	// The server transport uses the value of the top Via header field in
	// order to determine where to send a response.
	
	// If the host portion of the "sent-by" parameter
	// contains a domain name, or if it contains an IP address that differs
	// from the packet source address, the server MUST add a "received"
	// parameter to that Via header field value.
	// Via: SIP/2.0/UDP bobspc.biloxi.com:5060;received=192.0.2.4

	via = sip_vias_get(&t->reply->vias, 0);
	if (!via) return -1; // invalid via

	return t->handler->send(t->param, cstrvalid(&via->received) ? &via->received : &via->host, t->data, t->size);
}

int sip_uas_transaction_terminated(struct sip_uas_transaction_t* t)
{
	t->status = SIP_UAS_TRANSACTION_TERMINATED;

	if (t->timerh)
	{
		sip_uas_stop_timer(t->agent, t, t->timerh);
		t->timerh = NULL;
	}
	if (t->timerg)
	{
		sip_uas_stop_timer(t->agent, t, t->timerg);
		t->timerg = NULL;
	}
	if (t->timerij)
	{
		sip_uas_stop_timer(t->agent, t, t->timerij);
		t->timerij = NULL;
	}

	sip_uas_del_transaction(t->agent, t);
	return 0;
}

void sip_uas_transaction_ontimeout(void* usrptr)
{
	struct sip_uas_transaction_t* t;
	t = (struct sip_uas_transaction_t*)usrptr;
	locker_lock(&t->locker);
	t->timerh = NULL;

	if (t->status < SIP_UAS_TRANSACTION_CONFIRMED)
	{
		sip_uas_transaction_terminated(t);

		// TODO:
		// If a UAS generates a 2xx response and never receives an ACK, it
		// SHOULD generate a BYE to terminate the dialog.

		// 8.1.3.1 Transaction Layer Errors (p42)
		t->handler->onack(t->param, NULL, t, t->dialog->session, t->dialog, 408/*Invite Timeout*/, NULL, 0);
	}

	locker_unlock(&t->locker);
	sip_uas_transaction_release(t);
}

static void sip_uas_transaction_onterminated(void* usrptr)
{
	struct sip_uas_transaction_t* t;
	t = (struct sip_uas_transaction_t*)usrptr;

	locker_lock(&t->locker);
	if(SIP_UAS_TRANSACTION_TERMINATED != t->status)
		sip_uas_transaction_terminated(t);
	locker_unlock(&t->locker);
	sip_uas_transaction_release(t);
}

int sip_uas_transaction_timewait(struct sip_uas_transaction_t* t, int timeout)
{
	if (SIP_UAS_TRANSACTION_TERMINATED == t->status)
		return 0;

	if (t->timerh)
	{
		sip_uas_stop_timer(t->agent, t, t->timerh);
		t->timerh = NULL;
	}
	if (t->timerg)
	{
		sip_uas_stop_timer(t->agent, t, t->timerg);
		t->timerg = NULL;
	}

	assert(NULL == t->timerij);
	t->timerij = sip_uas_start_timer(t->agent, t, timeout, sip_uas_transaction_onterminated);
	return t->timerij ? 0 : -1;
}
