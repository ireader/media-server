#include "sip-uas-transaction.h"

static int sip_uas_transaction_timer_retransmission(void* id, void* usrptr);
static int sip_uas_transaction_timer_confirmed(void* id, void* usrptr);
static int sip_uas_transaction_timer_timeout(void* id, void* usrptr);

// Figure 7: INVITE server transaction (p136)
int sip_uas_transaction_invite_input(struct sip_uas_transaction_t* t, struct sip_dialog_t* dialog, const struct sip_message_t* req)
{
	int r;

	switch (t->status)
	{
	case SIP_UAS_TRANSACTION_INIT:
		// notify user
		t->status = SIP_UAS_TRANSACTION_TRYING;
		t->session = t->handler->oninvite(t->param, t, req->payload, req->size);
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
		//t->uas->handler->send(t->uas->param, "PROCEEDING");
		sip_uas_transaction_dosend(t);
		// ignore transport error(client will retransmission request)
		break;

	case SIP_UAS_TRANSACTION_COMPLETED:
		// do nothing
		// internal timer will send completed reply repeat
		// until recv ack
		if (!sip_message_isack(req))
			break;

		t->status = SIP_UAS_TRANSACTION_CONFIRMED;
		// continue to do more action

	case SIP_UAS_TRANSACTION_CONFIRMED:
		assert(sip_message_isack(req));
		t->status = SIP_UAS_TRANSACTION_TERMINATED;
		sip_uas_stop_timer(t->uas, t->timerg);
		sip_uas_stop_timer(t->uas, t->timerh);

		r = t->handler->onack(t->param, t, t->session, dialog, 200, req->payload, req->size);

		// start timer I, wait for all inflight ACK
		if (!t->reliable)
			t->timerij = sip_uas_start_timer(t->uas, T4, sip_uas_transaction_timer_confirmed, t);
		else
			sip_uas_transaction_release(t);

		return r;
		
	case SIP_UAS_TRANSACTION_TERMINATED:
		assert(sip_message_isack(req));
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
	if (SIP_UAS_TRANSACTION_TRYING != t->status && SIP_UAS_TRANSACTION_PROCEEDING == t->status)
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
	else if (200 <= code && code < 700)
	{
		t->status = SIP_UAS_TRANSACTION_COMPLETED;
	}
	else
	{
		assert(0);
		return -1; // invalid code
	}

	r = sip_uas_transaction_dosend(t);
	if (SIP_UAS_TRANSACTION_COMPLETED == t->status)
	{
		t->retries = 1;
		t->timerh = sip_uas_start_timer(t->uas, 64 * T1, sip_uas_transaction_timer_timeout, t);
		if (!t->reliable) // UDP
			t->timerg = sip_uas_start_timer(t->uas, T1, sip_uas_transaction_timer_retransmission, t);
	}

	return r;
}

static int sip_uas_transaction_timer_retransmission(void* id, void* usrptr)
{
	int r;
	struct sip_uas_transaction_t* t;
	t = (struct sip_uas_transaction_t*)usrptr;
	assert(t->timerg == id);

	locker_lock(&t->locker);
	t->timerg = NULL;
	r = sip_uas_transaction_dosend(t);
	//if (0 != r)
	//{
	//	locker_unlock(&t->locker);

	//	// 8.1.3.1 Transaction Layer Errors (p42)
	//	r = t->handler.onack(t->param, t, t->session, t->dialog, 503/*Service Unavailable*/, NULL, 0);
	//	locker_lock(&t->locker);
	//}

	if(!t->reliable)
		t->timerg = sip_uas_start_timer(t->uas, min(t->t2, T1 * (1 << t->retries++)), sip_uas_transaction_timer_retransmission, t);
	locker_unlock(&t->locker);
	return r;
}

static int sip_uas_transaction_timer_timeout(void* id, void* usrptr)
{
	int r;
	struct sip_uas_transaction_t* t;
	t = (struct sip_uas_transaction_t*)usrptr;
	assert(t->timerh == id);
	locker_lock(&t->locker);
	t->timerh = NULL;
	locker_unlock(&t->locker);

	// 8.1.3.1 Transaction Layer Errors (p42)
	r = t->handler->onack(t->param, t, t->session, NULL, 408/*Invite Timeout*/, NULL, 0);
	sip_uas_transaction_release(t);
	return r;
}

static int sip_uas_transaction_timer_confirmed(void* id, void* usrptr)
{
	struct sip_uas_transaction_t* t;
	t = (struct sip_uas_transaction_t*)usrptr;
	assert(t->timerij == id);
	locker_lock(&t->locker);
	t->timerij = NULL;
	locker_unlock(&t->locker);

	// all done
	sip_uas_transaction_release(t);
	return 0;
}
