#include "sip-uac-transaction.h"
#include "sip-transport.h"
#include "uri-parse.h"

struct sip_uac_transaction_t* sip_uac_transaction_create(struct sip_uac_t* uac, struct sip_message_t* req)
{
	struct sip_uac_transaction_t* t;
	t = (struct sip_uac_transaction_t*)calloc(1, sizeof(*t));
	if (NULL == t) return NULL;

	t->ref = 1;
	t->uac = uac;
	t->req = req;
	LIST_INIT_HEAD(&t->link);
	locker_create(&t->locker);
	t->status = SIP_UAC_TRANSACTION_CALLING;

	// 17.1.1.1 Overview of INVITE Transaction (p125)
	// For unreliable transports (such as UDP), the client transaction retransmits 
	// requests at an interval that starts at T1 seconds and doubles after every retransmission.
	// 17.1.2.1 Formal Description (p130)
	// For unreliable transports, requests are retransmitted at an interval which starts at T1 and doubles until it hits T2.
	t->t2 = sip_message_isinvite(req) ? (64 * T1) : T2;
	
	sip_uac_add_transaction(uac, t);
	return t;
}

int sip_uac_transaction_release(struct sip_uac_transaction_t* t)
{
	assert(t->ref > 0);
	if (0 != atomic_decrement32(&t->ref))
		return 0;

	assert(0 == t->ref);
	assert(NULL == t->timera);
	assert(NULL == t->timerb);
	assert(NULL == t->timerk);

	// unlink from uac
	sip_uac_del_transaction(t->uac, t);

	// MUST: destroy t->req after sip_uac_del_transaction
	sip_message_destroy(t->req);

	locker_destroy(&t->locker);
	free(t);
	return 0;
}

int sip_uac_transaction_addref(struct sip_uac_transaction_t* t)
{
	atomic_increment32(&t->ref);
	return 0;
}

static int sip_uac_transaction_dosend(struct sip_uac_transaction_t* t)
{
	return t->transport->send(t->transportptr, t->data, t->size);
}

static int sip_uac_transaction_timer_retransmission(void* id, void* usrptr)
{
	int r;
	struct sip_uac_transaction_t* t;
	t = (struct sip_uac_transaction_t*)usrptr;
	assert(t->timera == id);

	locker_lock(&t->locker);
	t->timera = NULL;
	r = sip_uac_transaction_dosend(t);
	if (0 != r)
	{
		locker_unlock(&t->locker);

		// 8.1.3.1 Transaction Layer Errors (p42)
		if (t->oninvite)
			return t->oninvite(t->param, t, NULL, 503/*Service Unavailable*/);
		else
			return t->onreply(t->param, t, 503/*Service Unavailable*/);
	}
	
	t->timera = sip_uac_start_timer(t->uac, min(t->t2, T1 * (1<<t->retries++)), sip_uac_transaction_timer_retransmission, t);
	locker_unlock(&t->locker);
	return r;
}

static int sip_uac_transaction_timer_timeout(void* id, void* usrptr)
{
	int r;
	struct sip_uac_transaction_t* t;
	t = (struct sip_uac_transaction_t*)usrptr;
	assert(t->timerb == id);
	locker_lock(&t->locker);
	t->timerb = NULL;
	locker_unlock(&t->locker);

	// 8.1.3.1 Transaction Layer Errors (p42)
	if(t->oninvite)
		r = t->oninvite(t->param, t, NULL, 408/*Request Timeout*/);
	else
		r = t->onreply(t->param, t, 408/*Request Timeout*/);

	sip_uac_transaction_release(t);
	return r;
}

int sip_uac_transaction_send(struct sip_uac_transaction_t* t)
{
	int r;
	locker_lock(&t->locker);
	r = sip_uac_transaction_dosend(t);
	if (r >= 0)
	{
		t->retries = 1;
		t->timerb = sip_uac_start_timer(t->uac, 64 * T1, sip_uac_transaction_timer_timeout, t);
		//if (0 == t->uac->transport->reliable(t->uac->transportptr))
		if(0 == r) // UDP, transport reliable == false
			t->timera = sip_uac_start_timer(t->uac, T1, sip_uac_transaction_timer_retransmission, t);
	}
	locker_unlock(&t->locker);

	return r >= 0 ? 0 : r;
	// TODO: return 503/*Service Unavailable*/
}
