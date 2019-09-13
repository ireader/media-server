#include "sip-uas-transaction.h"

#define MIN(x, y) ((x) < (y) ? (x) : (y))

static void sip_uas_transaction_onretransmission(void* usrptr);

static struct sip_dialog_t* sip_uas_create_dialog(const struct sip_message_t* req)
{
    struct sip_dialog_t* dialog;
    dialog = sip_dialog_create();
    if(!dialog)
        return NULL; // memory error
    
    if(0 != sip_dialog_init_uas(dialog, req))
    {
        assert(0);
        sip_dialog_release(dialog);
        return NULL;
    }
    
    return dialog;
}

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
        if (!dialog)
        {
            dialog = sip_uas_create_dialog(req);
            if(!dialog) return 0;
        }
        else
        {
            sip_dialog_addref(dialog); // for t->dialog
        }
            
        t->dialog = dialog;
		// notify user
		t->status = SIP_UAS_TRANSACTION_TRYING;
		// re-invite: 488 (Not Acceptable Here)
		t->dialog->session = t->handler->oninvite(t->param, req, t, (dialog && dialog->state == DIALOG_CONFIRMED) ? dialog : NULL, req->payload, req->size);
		// TODO: add timer here, send 100 trying
		if (!t->dialog->session && SIP_UAS_TRANSACTION_TRYING == t->status)
		{
			// user ignore/discard
			t->status = SIP_UAS_TRANSACTION_TERMINATED;
			sip_uas_transaction_timewait(t, TIMER_H);
		}
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
		// send last 1xx reply
		r = sip_uas_transaction_dosend(t);
		// ignore transport error(client will retransmission request)
		break;

	case SIP_UAS_TRANSACTION_COMPLETED:
		// do nothing
		// internal timer will send completed reply repeat
		// until recv ack
		
		// send last 3xx-6xx reply
		r = sip_uas_transaction_dosend(t);
		break;

	case SIP_UAS_TRANSACTION_ACCEPTED:
		// While in the "Accepted" state, any retransmissions of the INVITE
		// received will match this transaction state machine and will be
		// absorbed by the machine without changing its state. These
		// retransmissions are not passed onto the TU.

		// send last 2xx reply
		r = sip_uas_transaction_dosend(t);
		break;

	case SIP_UAS_TRANSACTION_CONFIRMED:
        assert(!dialog || dialog == t->dialog);
		if (oldstatus == SIP_UAS_TRANSACTION_ACCEPTED)
		{
            // update dialog target
            sip_dialog_target_refresh(t->dialog, req);
            
			if(t->dialog->state == DIALOG_ERALY) // re-invite
                t->dialog->state = DIALOG_CONFIRMED;
			r = t->handler->onack(t->param, req, t, t->dialog->session, t->dialog, 200, req->payload, req->size);

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

        break;

	case SIP_UAS_TRANSACTION_TERMINATED:
		// do nothing, wait for timer-I
		break;

	default:
		assert(0);
	}

	return r;
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

    // aet early dialog local url tag
    if(sip_message_isinvite(t->reply) && t->dialog && !cstrvalid(&t->dialog->local.uri.tag))
    {
        assert(cstrvalid(&t->reply->to.tag));
        sip_dialog_setlocaltag(t->dialog, &t->reply->to.tag);
        r = sip_dialog_add(t->agent, t->dialog);
        assert(0 == r);
    }

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
    
	if (SIP_UAS_TRANSACTION_COMPLETED == t->status || SIP_UAS_TRANSACTION_ACCEPTED == t->status)
	{
		// If the SIP element's TU (Transaction User) issues a 2xx response for
		// this transaction while the state machine is in the "Proceeding"
		// state, the state machine MUST transition to the "Accepted" state and
		// set Timer L to 64*T1

		t->retries = 1;
		t->timerh = sip_uas_start_timer(t->agent, t, TIMER_H, sip_uas_transaction_ontimeout);
		if (!t->reliable) // UDP
			t->timerg = sip_uas_start_timer(t->agent, t, TIMER_G, sip_uas_transaction_onretransmission);
		assert(t->timerh && (t->reliable || t->timerg));
	}

    return sip_uas_transaction_dosend(t);
}

static void sip_uas_transaction_onretransmission(void* usrptr)
{
	int r;
	struct sip_uas_transaction_t* t;
	t = (struct sip_uas_transaction_t*)usrptr;
	r = 0;
	locker_lock(&t->locker);
	t->timerg = NULL;

	if (t->status < SIP_UAS_TRANSACTION_CONFIRMED)
	{
		assert(SIP_UAS_TRANSACTION_COMPLETED == t->status || SIP_UAS_TRANSACTION_ACCEPTED == t->status);
		r = sip_uas_transaction_dosend(t);
		//if (0 != r)
		//{
		//	// 8.1. 3.1 Transaction Layer Errors (p42)
		//	r = t->handler.onack(t->param, t, t->session, t->dialog, 503/*Service Unavailable*/, NULL, 0);
		//}

		assert(!t->reliable);
		t->timerg = sip_uas_start_timer(t->agent, t, MIN(t->t2, T1 * (1 << t->retries++)), sip_uas_transaction_onretransmission);
	}

	locker_unlock(&t->locker);
	sip_uas_transaction_release(t);
}
