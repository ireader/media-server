#include "sip-uac-transaction.h"
#include "sip-uac.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int sip_uac_transaction_invite_proceeding(struct sip_uac_transaction_t* t, struct sip_dialog_t* dialog, const struct sip_message_t* reply)
{
	int r = 0;

	// 12.3 Termination of a Dialog (p77)
	// Independent of the method, if a request outside of a dialog generates
	// a non-2xx final response, any early dialogs created through
	// provisional responses to that request are terminated.
	if (SIP_UAC_TRANSACTION_COMPLETED == t->status)
		return 0; // 1. miss order, 2. fork invite

	assert(cstrvalid(&reply->to.tag));
	if (!dialog && cstrvalid(&reply->to.tag))
	{
		dialog = sip_dialog_create( reply);
		if (!dialog) return -1;
		sip_uac_add_dialog(t->uac, dialog);
	}

	// 17.1.1.2 Formal Description (p126)
	// the provisional response MUST be passed to the TU. 
	// Any further provisional responses MUST be passed up to the TU while in the "Proceeding" state.
	if (dialog && DIALOG_ERALY == dialog->state)
	{
		r = t->oninvite(t->param, t, dialog, reply->u.s.code);
	}
	else if (t->status <= SIP_UAC_TRANSACTION_PROCEEDING)
	{
		r = t->oninvite(t->param, t, dialog, reply->u.s.code);
	}
	else
	{
		// miss order reply, discard
	}

	// update status
	if (t->status == SIP_UAC_TRANSACTION_CALLING)
		t->status = SIP_UAC_TRANSACTION_PROCEEDING;

	return r;
}

static int sip_uac_transaction_invite_completed(struct sip_uac_transaction_t* t, struct sip_dialog_t* dialog, const struct sip_message_t* reply)
{
	int r = 0;

	// 17.1.1.2 Formal Description (p127)
	// Any retransmissions of the final response that are received while in
	// the "Completed" state MUST cause the ACK to be re-passed to the
	// transport layer for retransmission, but the newly received response
	// MUST NOT be passed up to the TU.

	if (dialog)
	{
		assert(DIALOG_ERALY == dialog->state);
		r = t->oninvite(t->param, t, dialog, reply->u.s.code);

		sip_uac_del_dialog(t->uac, dialog);
		sip_dialog_release(dialog);
	}
	else if (t->status < SIP_UAC_TRANSACTION_COMPLETED)
	{
		// only once
		r = t->oninvite(t->param, t, NULL, reply->u.s.code);
	}
	else
	{
		// retransmissions final response
	}

	t->status = SIP_UAC_TRANSACTION_COMPLETED;

	return r;
}

static int sip_uac_transaction_invite_accepted(struct sip_uac_transaction_t* t, struct sip_dialog_t* dialog, const struct sip_message_t* reply)
{
	int r = 0;
	if (!dialog)
	{
		dialog = sip_dialog_create(reply);
		if (!dialog) return -1;
		sip_uac_add_dialog(t->uac, dialog);
	}

	// completed To tag
	if (!cstrvalid(&dialog->remote.uri.tag))
		sip_dialog_setremotetag(dialog, &reply->to.tag);
	
	if (dialog->state != DIALOG_CONFIRMED)
	{
		dialog->state = DIALOG_CONFIRMED;

		r = t->oninvite(t->param, t, dialog, reply->u.s.code);
	}
	else
	{
		// miss order reply, discard
	}

	// rfc6026
	// An element encountering an unrecoverable transport error when trying
	// to send a response to an INVITE request MUST NOT immediately destroy
	// the associated INVITE server transaction state.  This state is
	// necessary to ensure correct processing of retransmissions of the request.
	if (t->status < SIP_UAC_TRANSACTION_ACCEPTED)
		t->status = SIP_UAC_TRANSACTION_ACCEPTED;

	if (0 == r)
	{
		// 1. start timer D (deprecated)
		// 2. send ACK
		r = sip_uac_ack(t, dialog);
	}

	return r;
}

// Figure 5: INVITE client transaction (p128)
int sip_uac_transaction_invite_input(struct sip_uac_transaction_t* t, const struct sip_message_t* reply)
{
	int r;
	struct sip_dialog_t* dialog;

	locker_lock(&t->locker);

	switch (t->status)
	{
	case SIP_UAC_TRANSACTION_CALLING:
		// stop retry timer A
		if (NULL != t->timera)
		{
			sip_uac_stop_timer(t->uac, t->timera);
			t->timera = NULL;
		}

	case SIP_UAC_TRANSACTION_PROCEEDING:
	case SIP_UAC_TRANSACTION_COMPLETED:
	case SIP_UAC_TRANSACTION_ACCEPTED:
		dialog = sip_uac_find_dialog(t->uac, reply);

		if (100 <= reply->u.s.code && reply->u.s.code < 200)
		{
			r = sip_uac_transaction_invite_proceeding(t, dialog, reply);
		}
		else if (200 <= reply->u.s.code && reply->u.s.code < 300)
		{
			r = sip_uac_transaction_invite_accepted(t, dialog, reply);
		}
		else if (300 <= reply->u.s.code && reply->u.s.code < 700)
		{
			r = sip_uac_transaction_invite_completed(t, dialog, reply);
		}
		else
		{
			assert(0);
			r = -1;
		}
		break;

	case SIP_UAC_TRANSACTION_TERMINATED:
	default:
		assert(0);
		r = -1;
	}

	locker_unlock(&t->locker);
	return r;
}
