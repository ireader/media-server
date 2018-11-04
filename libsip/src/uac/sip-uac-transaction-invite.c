#include "sip-uac-transaction.h"
#include "sip-uac.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int sip_uac_transaction_invite_proceeding(struct sip_uac_transaction_t* t, struct sip_dialog_t* dialog, const struct sip_message_t* reply)
{
	// TODO: add dialog locker here
	if (!dialog && cstrvalid(&reply->to.tag))
	{
		// create early dialog
		dialog = sip_dialog_create(reply);
		if (!dialog) return -1;
		sip_dialog_add(dialog);
	}

	// 17.1.1.2 Formal Description (p126)
	// the provisional response MUST be passed to the TU. 
	// Any further provisional responses MUST be passed up to the TU while in the "Proceeding" state.
	return t->oninvite(t->param, reply, t, dialog, reply->u.s.code);
}

static int sip_uac_transaction_invite_completed(struct sip_uac_transaction_t* t, struct sip_dialog_t* dialog, const struct sip_message_t* reply, int retransmissions)
{
	int r;

	assert(!dialog || DIALOG_ERALY == dialog->state);

	if (!retransmissions)
	{
		// Any retransmissions of the final response that are received while in
		// the "Completed" state MUST cause the ACK to be re-passed to the
		// transport layer for retransmission, but the newly received response
		// MUST NOT be passed up to the TU
		r = t->oninvite(t->param, reply, t, dialog, reply->u.s.code);
	}
	else
	{
		// 1. miss order / in-flight
		// 2. fork invite
		r = 0;
	}

	// The ACK MUST be sent to the same address, port, 
	// and transport to which the original request was sent.
	sip_uac_ack(t, dialog, 0); // ignore ack transport layer error, retry send on retransmissions

	if (!retransmissions)
	{
		// The client transaction SHOULD start timer D when it enters the
		// "Completed" state, with a value of at least 32 seconds for unreliable
		// transports, and a value of zero seconds for reliable transports.

		// Timer D reflects the amount of time that the server transaction can
		// remain in the "Completed" state when unreliable transports are used.
		// This is equal to Timer H in the INVITE server transaction, whose
		// default is 64*T1. However, the client transaction does not know the
		// value of T1 in use by the server transaction, so an absolute minimum
		// of 32s is used instead of basing Timer D on T1.

		// transfer to terminated
		sip_uac_transaction_timewait(t, t->reliable ? 1 : TIMER_D);
	}

	// delete dialog on sip_uac_del_transaction

	return r;
}

static int sip_uac_transaction_invite_accepted(struct sip_uac_transaction_t* t, struct sip_dialog_t* dialog, const struct sip_message_t* reply, int retransmissions)
{
	int r = 0;

	// TODO: add dialog locker here

	// only create new dialog on first 2xx response
	if (!dialog && !retransmissions)
	{
		dialog = sip_dialog_create(reply);
		if (!dialog) return -1;
		sip_dialog_add(dialog);
	}

	if (!dialog)
		return 0; // ignore fork response

	if (dialog->state == DIALOG_ERALY)
	{
		// transfer dialog state
		dialog->state = DIALOG_CONFIRMED;
		
		// completed To tag
		if (!cstrvalid(&dialog->remote.uri.tag))
			sip_dialog_setremotetag(dialog, &reply->to.tag);

		// receive and pass to the TU any retransmissions of the 2xx
		// response or any additional 2xx responses from other branches of a
		// downstream fork of the matching request.
		r = t->oninvite(t->param, reply, t, dialog, reply->u.s.code);
	}

	// rfc6026
	// An element encountering an unrecoverable transport error when trying
	// to send a response to an INVITE request MUST NOT immediately destroy
	// the associated INVITE server transaction state.  This state is
	// necessary to ensure correct processing of retransmissions of the request.
	sip_uac_ack(t, dialog, 1); // ignore ack transport layer error, retry send on retransmissions
	
	if (!retransmissions)
	{
		// If a 2xx response is received while the client INVITE state machine is 
		// in the "Calling" or "Proceeding" states, it MUST transition to the 
		// "Accepted" state, pass the 2xx response to the TU, and set Timer M to 64*T1.
		sip_uac_transaction_timewait(t, TIMER_M);
	}

	return r;
}

// Figure 5: INVITE client transaction (p128)
static int sip_uac_transaction_inivte_change_state(struct sip_uac_transaction_t* t, const struct sip_message_t* reply)
{
	// 17.1.1.2 Formal Description
	switch (t->status)
	{
	case SIP_UAC_TRANSACTION_CALLING:
	case SIP_UAC_TRANSACTION_PROCEEDING:
		assert(100 <= reply->u.s.code && reply->u.s.code < 700);
		if (100 <= reply->u.s.code && reply->u.s.code < 200)
			t->status = SIP_UAC_TRANSACTION_PROCEEDING;
		else if (200 <= reply->u.s.code && reply->u.s.code < 300)
			t->status = SIP_UAC_TRANSACTION_ACCEPTED;
		else if (200 <= reply->u.s.code && reply->u.s.code < 700)
			t->status = SIP_UAC_TRANSACTION_COMPLETED;
		break;

	case SIP_UAC_TRANSACTION_COMPLETED:
		assert(100 <= reply->u.s.code && reply->u.s.code < 700);
		break;

	case SIP_UAC_TRANSACTION_ACCEPTED:
		assert(100 <= reply->u.s.code && reply->u.s.code < 700);
		break;

	case SIP_UAC_TRANSACTION_TERMINATED:
		// nothing to do
		break;

	default:
		assert(0);
	}

	return t->status;
}

int sip_uac_transaction_invite_input(struct sip_uac_transaction_t* t, const struct sip_message_t* reply)
{
	int r, status, oldstatus;
	struct sip_dialog_t* dialog;
	
	// stop retry timer A
	if (NULL != t->timera)
	{
		sip_uac_stop_timer(t->uac, t, t->timera);
		t->timera = NULL;
	}
	dialog = sip_dialog_find(&reply->callid, &reply->from.tag, &reply->to.tag);

	oldstatus = t->status;
	status = sip_uac_transaction_inivte_change_state(t, reply);

	r = 0;
	switch (status)
	{
	case SIP_UAC_TRANSACTION_CALLING:
	case SIP_UAC_TRANSACTION_PROCEEDING:
		r = sip_uac_transaction_invite_proceeding(t, dialog, reply);
		break;

	case SIP_UAC_TRANSACTION_ACCEPTED:
		if (200 <= reply->u.s.code && reply->u.s.code < 300)
			r = sip_uac_transaction_invite_accepted(t, dialog, reply, oldstatus == status);
		// ignore other status code (fork)
		break;

	case SIP_UAC_TRANSACTION_COMPLETED:
		// ignore other status code (fork or misorder)
		r = sip_uac_transaction_invite_completed(t, dialog, reply, oldstatus == status);
		
		// TODO: fork 2xx response
		//if (200 <= reply->u.s.code && reply->u.s.code < 300)
		break;

	case SIP_UAC_TRANSACTION_TERMINATED:
	default:
		assert(0);
		break;
	}

	return r;
}
