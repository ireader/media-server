#include "sip-uac-transaction.h"
#include "sip-transport.h"
#include "uri-parse.h"

#define min(x, y) ((x) < (y) ? (x) : (y))

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

	// unlink from uac
	sip_uac_del_transaction(t->uac, t);

	assert(0 == t->ref);
	assert(NULL == t->timera);
	assert(NULL == t->timerb);
	assert(NULL == t->timerk);

	// MUST: destroy t->req after sip_uac_del_transaction
	assert(t->link.next == t->link.prev);
	sip_message_destroy(t->req);

	locker_destroy(&t->locker);
	free(t);
	return 0;
}

int sip_uac_transaction_addref(struct sip_uac_transaction_t* t)
{
	return atomic_increment32(&t->ref);
}

//static int sip_uac_transaction_destroy(struct sip_uac_transaction_t* t)
//{
//	// unlink from uac
//	sip_uac_del_transaction(t->uac, t);
//
//	return sip_uac_transaction_release(t);
//}

static int sip_uac_transaction_dosend(struct sip_uac_transaction_t* t)
{
	return t->transport.send(t->transportptr, t->data, t->size);
}

static int sip_uac_transaction_onretransmission(void* usrptr)
{
	int r;
	struct sip_uac_transaction_t* t;
	t = (struct sip_uac_transaction_t*)usrptr;
	locker_lock(&t->locker);

	r = sip_uac_transaction_dosend(t);
	if (0 != r)
	{
		// ignore retransmission error

		//// 8.1.3.1 Transaction Layer Errors (p42)
		//if (t->oninvite)
		//	r = t->oninvite(t->param, t, NULL, 503/*Service Unavailable*/);
		//else
		//	r = t->onreply(t->param, t, 503/*Service Unavailable*/);
	}
	
	if (t->timera)
		sip_uac_stop_timer(t->uac, t, t->timera);
	t->timera = sip_uac_start_timer(t->uac, t, min(t->t2, T1 * (1 << t->retries++)), sip_uac_transaction_onretransmission);

	locker_unlock(&t->locker);
	sip_uac_transaction_release(t);
	return 0;
}

static int sip_uac_transaction_terminate(struct sip_uac_transaction_t* t)
{
	t->status = SIP_UAC_TRANSACTION_TERMINATED;

	if (t->timera)
	{
		sip_uac_stop_timer(t->uac, t, t->timera);
		t->timera = NULL;
	}
	if (t->timerb)
	{
		sip_uac_stop_timer(t->uac, t, t->timerb);
		t->timerb = NULL;
	}
	if (t->timerk)
	{
		sip_uac_stop_timer(t->uac, t, t->timerk);
		t->timerk = NULL;
	}

	sip_uac_transaction_release(t);
	return 0;
}

static int sip_uac_transaction_onterminate(void* usrptr)
{
	struct sip_uac_transaction_t* t;
	t = (struct sip_uac_transaction_t*)usrptr;
	locker_lock(&t->locker);
	sip_uac_transaction_terminate(t);
	locker_unlock(&t->locker);
	sip_uac_transaction_release(t);
	return 0;
}

static int sip_uac_transaction_ontimeout(void* usrptr)
{
	int r;
	struct sip_uac_transaction_t* t;
	t = (struct sip_uac_transaction_t*)usrptr;
	locker_lock(&t->locker);

	// 8.1.3.1 Transaction Layer Errors (p42)
	if (t->oninvite)
		r = t->oninvite(t->param, NULL, t, NULL, 408/*Request Timeout*/);
	else
		r = t->onreply(t->param, NULL, t, 408/*Request Timeout*/);

	sip_uac_transaction_terminate(t);
	locker_unlock(&t->locker);
	sip_uac_transaction_release(t);
	return r;
}

int sip_uac_transaction_send(struct sip_uac_transaction_t* t)
{
	int r;
	r = sip_uac_transaction_dosend(t);
	if (0 == r)
	{
		t->retries = 1; // reset retry times
		t->timerb = sip_uac_start_timer(t->uac, t, TIMER_B, sip_uac_transaction_ontimeout);
		if(!t->reliable) // UDP
			t->timera = sip_uac_start_timer(t->uac, t, TIMER_A, sip_uac_transaction_onretransmission);
		assert(t->timerb && (!t->reliable || t->timera));
	}
	
	// TODO: return 503/*Service Unavailable*/
	return r;
}

int sip_uac_transaction_timewait(struct sip_uac_transaction_t* t, int timeout)
{
	if (t->timera)
	{
		sip_uac_stop_timer(t->uac, t, t->timera);
		t->timera = NULL;
	}
	if (t->timerb)
	{
		sip_uac_stop_timer(t->uac, t, t->timerb);
		t->timerb = NULL;
	}
	if (t->timerk)
	{
		sip_uac_stop_timer(t->uac, t, t->timerk);
		t->timerk = NULL;
	}

	t->timerk = sip_uac_start_timer(t->uac, t, timeout, sip_uac_transaction_onterminate);
	return t->timerk ? 0 : -1;
}
