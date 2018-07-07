#include "sip-uac.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int sip_uac_transaction_invite_input(struct sip_uac_transaction_t* t, const struct sip_message_t* msg)
{
	switch (t->status)
	{
	case SIP_UAC_TRANSACTION_CALLING:
	case SIP_UAC_TRANSACTION_PROCEEDING:
		if (100 <= msg->u.s.code && msg->u.s.code < 200)
		{
			return sip_uac_transaction_invite_onproceeding(t, msg);
		}
		else if (200 <= msg->u.s.code && msg->u.s.code < 300)
		{
			return sip_uac_transaction_invite_onterminated(t, msg);
		}
		else if (300 <= msg->u.s.code && msg->u.s.code < 700)
		{
			return sip_uac_transaction_invite_oncompleted(t, msg);
		}
		else
		{
			assert(0);
			return -1;
		}

	case SIP_UAC_TRANSACTION_COMPLETED:
		if (300 <= msg->u.s.code && msg->u.s.code < 700)
		{
			return sip_uac_transaction_invite_oncompleted(t, msg);
		}
		else if (100 <= msg->u.s.code && msg->u.s.code < 200)
		{
			// multi-target, ignore
		}
		else if (200 <= msg->u.s.code && msg->u.s.code < 300)
		{
			// multi-target, notify UA Core directly
		}
		else
		{
			assert(0);
			return -1;
		}

	default:
		assert(0);
		return -1;
	}
}

int sip_uac_transaction_invite_send(struct sip_uac_transaction_t* t)
{
	int r;
	if (cstrcasecmp(&t->msg->u.c.method, "invite"))
	{
		t->status = SIP_UAC_TRANSACTION_CALLING;
		t->timera = t->uac->timer.start(t->uac->timerptr, TIMER_A, sip_uac_transaction_invite_ontimer, t);
		t->timerb = t->uac->timer.start(t->uac->timerptr, TIMER_B, sip_uac_transaction_invite_ontimer, t);
		r = t->uac->transport->send(t->uac->transportptr, t->data, t->size);
	}
	else
	{
		t->status = SIP_UAC_TRANSACTION_TRYING;
		t->timera = t->uac->timer.start(t->uac->timerptr, TIMER_E, sip_uac_transaction_invite_ontimer, t);
		t->timerb = t->uac->timer.start(t->uac->timerptr, TIMER_F, sip_uac_transaction_invite_ontimer, t);
		r = t->uac->transport->send(t->uac->transportptr, t->data, t->size);
	}
}

static int sip_uac_transaction_invite_onproceeding(struct sip_uac_transaction_t* t, const struct sip_message_t* msg)
{
	locker_lock(&t->locker);
	assert(SIP_UAC_TRANSACTION_CALLING == t->status || SIP_UAC_TRANSACTION_PROCEEDING == t->status);
	t->status = SIP_UAC_TRANSACTION_PROCEEDING;
	t->uac->timer.stop(t->uac->timerptr, t->timera);
	r = t->uac->oncalling(t, msg);
	locker_unlock(&t->locker);
}

static int sip_uac_transaction_invite_oncompleted(struct sip_uac_transaction_t* t, const struct sip_message_t* msg)
{
	locker_lock(&t->locker);
	assert(SIP_UAC_TRANSACTION_CALLING == t->status || SIP_UAC_TRANSACTION_PROCEEDING == t->status || SIP_UAC_TRANSACTION_COMPLETED == t->status);
	t->status = SIP_UAC_TRANSACTION_COMPLETED;
	t->timerd = t->uac->timer.start(t->uac->timerptr, TIMER_D, sip_uac_transaction_invite_ontimer, t);
	r = t->uac->transport->send(ACK);
	locker_unlock(&t->locker);
	return 0;
}

// only once
static int sip_uac_transaction_invite_onterminated(struct sip_uac_transaction_t* t, const struct sip_message_t* msg)
{
	locker_lock(&t->locker);
	assert(SIP_UAC_TRANSACTION_TERMINATED != t->status);
	t->status = SIP_UAC_TRANSACTION_TERMINATED;
	r = t->uac->onreply(t, msg);
	locker_unlock(&t->locker);

	sip_uac_transaction_destroy(t);
	return r;
}

static int sip_uac_transaction_invite_ontimer(void* id, void* usrptr)
{
	int r;
	struct sip_uac_transaction_t* t;
	t = (struct sip_uac_transaction_t*)usrptr;
	assert(t->timera == id || t->timerb == id || t->timerd == id);

	if (t->timera == id)
	{
		// exponential backoffs on retransmissions
		// For unreliable transports (such as UDP), the client transaction retransmits requests at an
		// interval that starts at T1 seconds and doubles after every retransmission.
		locker_lock(&t->locker);
		t->timera = t->uac->timer.start(t->uac->timerptr, TIMER_A * (1 << ++t->retransmission), sip_uac_transaction_invite_onterminated, t);
		r = t->uac->transport->send(t->uac->transportptr, t->data, t->size);
		locker_unlock(&t->locker);
	}
	else if (t->timerb == id || t->timerd == id)
	{
		// Terminated
		locker_lock(&t->locker);
		t->uac->timer.stop(t->uac->timerptr, t->timera);
		t->uac->timer.stop(t->uac->timerptr, t->timerb);
		t->uac->timer.stop(t->uac->timerptr, t->timerd);
		sip_uac_transaction_invite_onterminated(t);
		locker_unlock(&t->locker);
	}
	else
	{
		assert(0);
	}

	return 0;
}
