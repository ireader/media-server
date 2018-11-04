#include "sip-uas-transaction.h"
#include "sip-transport.h"

struct sip_uas_transaction_t* sip_uas_transaction_create(struct sip_uas_t* uas, const struct sip_message_t* req)
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
	t->uas = uas;
	LIST_INIT_HEAD(&t->link);
	locker_create(&t->locker);
	t->status = SIP_UAS_TRANSACTION_INIT;

	// 17.1.1.1 Overview of INVITE Transaction (p125)
	// For unreliable transports (such as UDP), the client transaction retransmits 
	// requests at an interval that starts at T1 seconds and doubles after every retransmission.
	// 17.1.2.1 Formal Description (p130)
	// For unreliable transports, requests are retransmitted at an interval which starts at T1 and doubles until it hits T2.
	t->t2 = sip_message_isinvite(req) ? (64 * T1) : T2;

	sip_uas_add_transaction(uas, t);
	return t;
}

int sip_uas_transaction_release(struct sip_uas_transaction_t* t)
{
	assert(t->ref > 0);
	if (0 != atomic_decrement32(&t->ref))
		return 0;
	
	// unlink from uas
	sip_uas_del_transaction(t->uas, t);

	assert(0 == t->ref);
	assert(NULL == t->timerg);
	assert(NULL == t->timerh);
	assert(NULL == t->timerij);

	// MUST: destroy t->reply after sip_uas_del_transaction
	//sip_message_destroy((struct sip_message_t*)t->req);

	if (t->reply)
		sip_message_destroy(t->reply);

	locker_destroy(&t->locker);
	free(t);
	return 0;
}

int sip_uas_transaction_addref(struct sip_uas_transaction_t* t)
{
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
	if (0 == cstrcasecmp(&req->u.c.method, "CANCEL"))
	{
		return sip_uas_oncancel(t, dialog, req);
	}
	else if (0 == cstrcasecmp(&req->u.c.method, "BYE"))
	{
		return sip_uas_onbye(t, dialog, req);
	}
	else if (0 == cstrcasecmp(&req->u.c.method, "REGISTER"))
	{
		return sip_uas_onregister(t, req);
	}
	else if (0 == cstrcasecmp(&req->u.c.method, "OPTIONS"))
	{
		return sip_uas_onoptions(t, req);
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
	int r;
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

	r = t->handler->send(t->param, cstrvalid(&via->received) ? &via->received : &via->host, t->data, t->size);
	if (r >= 0)
		t->reliable = r;
	return r;
}

static int sip_uas_transaction_onterminated(void* usrptr)
{
	struct sip_uas_transaction_t* t;
	t = (struct sip_uas_transaction_t*)usrptr;

	locker_lock(&t->locker);
	t->status = SIP_UAS_TRANSACTION_TERMINATED;

	if (t->timerh)
	{
		sip_uas_stop_timer(t->uas, t, t->timerh);
		t->timerh = NULL;
	}
	if (t->timerg)
	{
		sip_uas_stop_timer(t->uas, t, t->timerg);
		t->timerg = NULL;
	}
	if (t->timerij)
	{
		sip_uas_stop_timer(t->uas, t, t->timerij);
		t->timerij = NULL;
	}
	locker_unlock(&t->locker);

	// all done
	sip_uas_transaction_release(t);
	return 0;
}

int sip_uas_transaction_timewait(struct sip_uas_transaction_t* t, int timeout)
{
	if (t->timerh)
	{
		sip_uas_stop_timer(t->uas, t, t->timerh);
		t->timerh = NULL;
	}
	if (t->timerg)
	{
		sip_uas_stop_timer(t->uas, t, t->timerg);
		t->timerg = NULL;
	}
	if (t->timerij)
	{
		sip_uas_stop_timer(t->uas, t, t->timerij);
		t->timerij = NULL;
	}

	t->timerij = sip_uas_start_timer(t->uas, t, timeout, sip_uas_transaction_onterminated);
	return t->timerij ? 0 : -1;
}
