#include "sip-uas-transaction.h"

#define min(x, y) ((x) < (y) ? (x) : (y))

static int sip_uas_transaction_onretransmission(void* usrptr);
static int sip_uas_transaction_ontimeout(void* usrptr);

// Figure 7: INVITE server transaction (p136)
static int sip_uas_transaction_inivte_change_state(struct sip_uas_transaction_t* t, const struct sip_message_t* req)
{
	switch (t->status)
	{
	case SIP_UAS_TRANSACTION_ACCEPTED:
	case SIP_UAS_TRANSACTION_COMPLETED:
		if (sip_message_isack(req))
			t->status = SIP_UAS_TRANSACTION_CONFIRMED;
		break;
	}

	return t->status;
}

int sip_uas_transaction_invite_input(struct sip_uas_transaction_t* t, struct sip_dialog_t* dialog, const struct sip_message_t* req)
{
	int r, status, oldstatus;

	oldstatus = t->status;
	status = sip_uas_transaction_inivte_change_state(t, req);

	r = 0;
	switch (status)
	{
	case SIP_UAS_TRANSACTION_INIT:
		// notify user
		t->dialog = dialog;
		t->status = SIP_UAS_TRANSACTION_TRYING;
		// re-invite: 488 (Not Acceptable Here)
		dialog->session = t->handler->oninvite(t->param, req, t, dialog->state == DIALOG_ERALY ? NULL : dialog, req->payload, req->size);
		break;

	case SIP_UAS_TRANSACTION_TRYING:
		// duplication message
		// discard and reply trying

		// When a 100 (Trying) response is generated, any Timestamp header field
		// present in the request MUST be copied into this 100 (Trying) response.
		// If there is a delay in generating the response, the UAS
		// SHOULD add a delay value into the Timestamp value in the response.
		//sip_uas_transaction_invite_reply(t, 100, NULL, 0);

		// goto proceeding on user rely
		//t->status = SIP_UAS_TRANSACTION_PROCEEDING;
		break;

	case SIP_UAS_TRANSACTION_PROCEEDING:
		// send last proceeding reply
		sip_uas_transaction_dosend(t);
		// ignore transport error(client will retransmission request)
		break;

	case SIP_UAS_TRANSACTION_COMPLETED:
		// do nothing
		// internal timer will send completed reply repeat
		// until recv ack
		break;

	case SIP_UAS_TRANSACTION_ACCEPTED:
		// While in the "Accepted" state, any retransmissions of the INVITE
		// received will match this transaction state machine and will be
		// absorbed by the machine without changing its state. These
		// retransmissions are not passed onto the TU.
		break;

	case SIP_UAS_TRANSACTION_CONFIRMED:
		if (oldstatus == SIP_UAS_TRANSACTION_ACCEPTED)
		{
			assert(dialog->state == DIALOG_ERALY); // re-invite
			dialog->state = DIALOG_CONFIRMED;

			r = t->handler->onack(t->param, req, t, dialog->session, dialog, 200, req->payload, req->size);

			// start timer I, wait for all inflight ACK
			sip_uas_transaction_timewait(t, t->reliable ? 1 : TIMER_I);
		}
		else if (oldstatus == SIP_UAS_TRANSACTION_COMPLETED)
		{
			// start timer I, wait for all inflight ACK
			sip_uas_transaction_timewait(t, t->reliable ? 1 : TIMER_I);
		}
		else
		{
			assert(oldstatus == SIP_UAS_TRANSACTION_CONFIRMED);
		}

		return r;

	case SIP_UAS_TRANSACTION_TERMINATED:
		// do nothing, wait for timer-I
		break;

	default:
		assert(0);
	}

	return 0;
}

int sip_uas_transaction_invite_reply(struct sip_uas_transaction_t* t, int code, const void* data, int bytes)
{
	int r;
	assert(SIP_UAS_TRANSACTION_TRYING == t->status || SIP_UAS_TRANSACTION_PROCEEDING == t->status);
	if (SIP_UAS_TRANSACTION_TRYING != t->status && SIP_UAS_TRANSACTION_PROCEEDING != t->status)
		return 0; // discard

	t->reply->u.s.code = code;
	t->reply->u.s.reason.p = sip_reason_phrase(code);
	t->reply->u.s.reason.n = strlen(t->reply->u.s.reason.p);
	t->reply->payload = data;
	t->reply->size = bytes;
	t->size = sip_message_write(t->reply, t->data, sizeof(t->data));
	if (t->size < 1)
		return -1;

	if (100 <= code && code < 200)
	{
		// provisional response
		t->status = SIP_UAS_TRANSACTION_PROCEEDING;
	}
	else if (200 <= code && code < 300)
	{
		// If a UAS generates a 2xx response and never receives an ACK, it
		// SHOULD generate a BYE to terminate the dialog.

		// rfc6026
		t->status = SIP_UAS_TRANSACTION_ACCEPTED;
	}
	else if (300 <= code && code < 700)
	{
		t->status = SIP_UAS_TRANSACTION_COMPLETED;
	}
	else
	{
		assert(0);
		return -1; // invalid code
	}

	r = sip_uas_transaction_dosend(t);
	if (SIP_UAS_TRANSACTION_COMPLETED == t->status || SIP_UAS_TRANSACTION_ACCEPTED == t->status)
	{
		// If the SIP element's TU (Transaction User) issues a 2xx response for
		// this transaction while the state machine is in the "Proceeding"
		// state, the state machine MUST transition to the "Accepted" state and
		// set Timer L to 64*T1

		t->retries = 1;
		t->timerh = sip_uas_start_timer(t->uas, t, TIMER_H, sip_uas_transaction_ontimeout);
		if (!t->reliable) // UDP
			t->timerg = sip_uas_start_timer(t->uas, t, TIMER_G, sip_uas_transaction_onretransmission);
		assert(t->timerh && (!t->reliable || t->timerg));
	}

	return r;
}

static int sip_uas_transaction_onretransmission(void* usrptr)
{
	int r;
	struct sip_uas_transaction_t* t;
	t = (struct sip_uas_transaction_t*)usrptr;
	locker_lock(&t->locker);

	t->timerg = NULL;
	r = sip_uas_transaction_dosend(t);
	//if (0 != r)
	//{
	//	// 8.1.3.1 Transaction Layer Errors (p42)
	//	r = t->handler.onack(t->param, t, t->session, t->dialog, 503/*Service Unavailable*/, NULL, 0);
	//}

	if(!t->reliable)
		t->timerg = sip_uas_start_timer(t->uas, t, min(t->t2, T1 * (1 << t->retries++)), sip_uas_transaction_onretransmission);

	locker_unlock(&t->locker);
	sip_uas_transaction_release(t);
	return r;
}

static int sip_uas_transaction_ontimeout(void* usrptr)
{
	int r;
	struct sip_uas_transaction_t* t;
	t = (struct sip_uas_transaction_t*)usrptr;
	locker_lock(&t->locker);
	t->timerh = NULL;
	
	// TODO:
	// If a UAS generates a 2xx response and never receives an ACK, it
	// SHOULD generate a BYE to terminate the dialog.

	// 8.1.3.1 Transaction Layer Errors (p42)
	r = t->handler->onack(t->param, NULL, t, t->dialog->session, t->dialog, 408/*Invite Timeout*/, NULL, 0);
	
	locker_unlock(&t->locker);
	sip_uas_transaction_release(t);
	return r;
}
