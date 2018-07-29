#include "sip-uac-transaction.h"
#include "sip-uac.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int sip_uac_transaction_noninvite_proceeding(struct sip_uac_transaction_t* t, const struct sip_message_t* reply)
{
	int r;
	locker_lock(&t->locker);
	assert(SIP_UAC_TRANSACTION_TRYING == t->status || SIP_UAC_TRANSACTION_PROCEEDING == t->status);
	r = t->h.noninvite(t->param, t, reply->u.s.code);
	locker_unlock(&t->locker);
	return r;
}

static int sip_uac_transaction_noninvite_completed(struct sip_uac_transaction_t* t, const struct sip_message_t* reply)
{
	int r;
	locker_lock(&t->locker);
	assert(SIP_UAC_TRANSACTION_TRYING == t->status || SIP_UAC_TRANSACTION_PROCEEDING == t->status || SIP_UAC_TRANSACTION_PROCEEDING == t->status || SIP_UAC_TRANSACTION_COMPLETED == t->status);
	t->h.noninvite(t->param, t, reply->u.s.code);
	if (!t->uac->transport->reliable(t->uac->transportptr))
	{
		t->timerk = t->uac->timer.start(t->uac->timerptr, TIMER_K, sip_uac_transaction_timer_timeout, t);
	}
	else
	{
		t->status = SIP_UAC_TRANSACTION_TERMINATED;
		sip_uac_transaction_timer_timeout(t->timerk, t);
	}
	locker_unlock(&t->locker);
	return 0;
}

int sip_uac_transaction_noninvite_input(struct sip_uac_transaction_t* t, const struct sip_message_t* reply)
{
	int r;
	struct sip_dialog_t* dialog;

	locker_lock(&t->locker);

	switch (t->status)
	{
	case SIP_UAC_TRANSACTION_TRYING:
	case SIP_UAC_TRANSACTION_PROCEEDING:
		if (100 <= reply->u.s.code && reply->u.s.code < 200)
		{
			t->status = SIP_UAC_TRANSACTION_PROCEEDING;
			return sip_uac_transaction_noninvite_proceeding(t, reply);
		}
		else if (200 <= reply->u.s.code && reply->u.s.code < 700)
		{
			t->status = SIP_UAC_TRANSACTION_COMPLETED;
			return sip_uac_transaction_noninvite_completed(t, reply);
		}
		else
		{
			assert(0);
			return -1;
		}
		break;

	case SIP_UAC_TRANSACTION_COMPLETED:
		// stop retry timer A
		if (NULL != t->timera)
		{
			t->uac->timer.stop(t->uac->timerptr, t->timera);
			t->timera = NULL;
		}

		// duplicated packet, discard
		assert(100 <= reply->u.s.code && reply->u.s.code < 700);
		break;

	case SIP_UAC_TRANSACTION_TERMINATED:
	default:
		assert(0);
		r = -1;
	}

	locker_unlock(&t->locker);
	return r;
}
