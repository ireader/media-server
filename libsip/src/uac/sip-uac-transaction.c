#include "sip-uac-transaction.h"
#include "sip-uac.h"

struct sip_uac_transaction_t* sip_uac_transaction_create(struct sip_uac_t* uac, const struct sip_message_t* msg)
{
	struct sip_uac_transaction_t* t;
	t = (struct sip_uac_transaction_t*)calloc(1, sizeof(*t));
	if (NULL == t) return NULL;

	atomic_increment32(&uac->ref);
	locker_create(&t->locker);
	t->msg = msg;
	t->uac = uac;
	t->status = SIP_UAC_TRANSACTION_CALLING;

	// 17.1.1.1 Overview of INVITE Transaction (p125)
	// For unreliable transports (such as UDP), the client transaction retransmits 
	// requests at an interval that starts at T1 seconds and doubles after every retransmission.
	// 17.1.2.1 Formal Description (p130)
	// For unreliable transports, requests are retransmitted at an interval which starts at T1 and doubles until it hits T2.
	t->t2 = sip_message_isinvite(msg) ? (64 * T1) : T2;

	// link to tail
	locker_lock(&uac->locker);
	list_insert_after(&t->link, uac->transactions.prev);
	locker_unlock(&uac->locker);

	// message
	t->size = sip_message_write(msg, t->data, sizeof(t->data));
	if (t->size < 0 || t->size >= sizeof(t->data))
	{
		sip_uac_transaction_destroy(uac, t);
		return NULL;
	}
	return t;
}

int sip_uac_transaction_destroy(struct sip_uac_transaction_t* t)
{
	struct sip_dialog_t* dialog;
	struct list_head *pos, *next;

	assert(NULL == t->timera);
	assert(NULL == t->timerb);

	// unlink from uac
	locker_lock(&t->uac->locker);
	list_remove(&t->link);
	
	// destroy all early dialog
	list_for_each_safe(pos, next, &t->uac->dialogs)
	{
		dialog = list_entry(pos, struct sip_dialog_t, link);
		if (cstreq(&dialog->callid, &t->msg->callid) && DIALOG_ERALY == dialog->state)
		{
			sip_dialog_destroy(dialog);
			list_remove(pos);
		}
	}
	locker_unlock(&t->uac->locker);

	locker_destroy(&t->locker);
	free(t);
	return 0;
}

static int sip_uac_transaction_timer_retransmission(void* id, void* usrptr)
{
	int r;
	struct sip_uac_transaction_t* t;
	t = (struct sip_uac_transaction_t*)usrptr;
	assert(t->timera == id);

	locker_lock(&t->locker);
	t->timera = NULL;
	r = t->uac->transport->send(t->uac->transportptr, t->data, r);
	if (0 != r)
	{
		locker_unlock(&t->locker);

		// 8.1.3.1 Transaction Layer Errors (p42)
		return t->handler(t->param, 503/*Service Unavailable*/);
	}
	
	t->timera = t->uac->timer.start(t->uac->timerptr, min(t->t2, T1 * (1<<t->retries++)), sip_uac_transaction_timer_retransmission, t);
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
	r = t->handler(t->param, 408/*Request Timeout*/);

	sip_uac_transaction_destroy(t);
	return r;
}

int sip_uac_transaction_send(struct sip_uac_transaction_t* t)
{
	int r;
	r = sip_message_write(t->msg, t->data, sizeof(t->data));
	if (0 != r)
		return r;

	locker_lock(&t->locker);
	r = t->uac->transport->send(t->uac->transportptr, t->data, r);
	if (0 == r)
	{
		t->retries = 1;
		t->timerb = t->uac->timer.start(t->uac->timerptr, 64 * T1, sip_uac_transaction_timer_timeout, t);
		if (0 == t->uac->transport->reliable(t->uac->transportptr))
			t->timera = t->uac->timer.start(t->uac->timerptr, T1, sip_uac_transaction_timer_retransmission, t);
	}
	locker_unlock(&t->locker);

	return r;
	// TODO: return 503/*Service Unavailable*/
}
