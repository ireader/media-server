/*
                                    |INVITE from TU
                  Timer A fires     |INVITE sent      Timer B fires
                  Reset A,          V                 or Transport Err.
                  INVITE sent +-----------+           inform TU
                    +---------|           |--------------------------+
                    |         |  Calling  |                          |
                    +-------->|           |-----------+              |
   300-699                    +-----------+ 2xx       |              |
   ACK sent                      |  |       2xx to TU |              |
   resp. to TU                   |  |1xx              |              |
   +-----------------------------+  |1xx to TU        |              |
   |                                |                 |              |
   |                1xx             V                 |              |
   |                1xx to TU +-----------+           |              |
   |                +---------|           |           |              |
   |                |         |Proceeding |           |              |
   |                +-------->|           |           |              |
   |                          +-----------+ 2xx       |              |
   |         300-699             |    |     2xx to TU |              |
   |         ACK sent,  +--------+    +---------------+              |
   |         resp. to TU|                             |              |
   |                    |                             |              |
   |                    V                             V              |
   |              +-----------+                   +----------+       |
   +------------->|           |Transport Err.     |          |       |
                  | Completed |Inform TU          | Accepted |       |
               +--|           |-------+           |          |-+     |
       300-699 |  +-----------+       |           +----------+ |     |
       ACK sent|    ^  |              |               |  ^     |     |
               |    |  |              |               |  |     |     |
               +----+  |              |               |  +-----+     |
                       |Timer D fires |  Timer M fires|    2xx       |
                       |-             |             - |    2xx to TU |
                       +--------+     |   +-----------+              |
      NOTE:                     V     V   V                          |
   Transitions                 +------------+                        |
   are labeled                 |            |                        |
   with the event              | Terminated |<-----------------------+
   over the action             |            |
   to take.                    +------------+

					Figure 5: INVITE client transaction
*/

#include "sip-uac-transaction.h"
#include "sip-uac.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int sip_uac_transaction_invite_proceeding(struct sip_uac_transaction_t* t, const struct sip_message_t* reply)
{
	int r;
	struct sip_dialog_t* dialog;

	dialog = sip_dialog_fetch(t->agent, &reply->callid, &reply->from.tag, &reply->to.tag);

	// TODO: add dialog locker here
	if (!dialog && cstrvalid(&reply->to.tag))
	{
		// create early dialog
		dialog = sip_dialog_create();
		if (!dialog) return -1;
		if (0 != sip_dialog_init_uac(dialog, reply) || 0 != sip_dialog_set_local_target(dialog, t->req) || 0 != sip_dialog_add(t->agent, dialog))
		{
			assert(0);
			sip_dialog_release(dialog);
			dialog = NULL;
			return 0; // ignore
		}
	}

	// 17.1.1.2 Formal Description (p126)
	// the provisional response MUST be passed to the TU. 
	// Any further provisional responses MUST be passed up to the TU while in the "Proceeding" state.
	r = t->oninvite(t->param, reply, t, dialog, reply->u.s.code, NULL); // ignore session

	// reset timer b for proceeding too long
    sip_uac_transaction_timeout(t, TIMER_B);
    sip_dialog_release(dialog);
    return r;
}

static int sip_uac_transaction_invite_completed(struct sip_uac_transaction_t* t, const struct sip_message_t* reply, int retransmissions)
{
	int r;
	struct sip_dialog_t* dialog;

	dialog = sip_dialog_fetch(t->agent, &reply->callid, &reply->from.tag, &reply->to.tag);
	assert(!dialog || DIALOG_ERALY == dialog->state);

	r = 0;
	if (!retransmissions)
	{
		// Any retransmissions of the final response that are received while in
		// the "Completed" state MUST cause the ACK to be re-passed to the
		// transport layer for retransmission, but the newly received response
		// MUST NOT be passed up to the TU
		assert(!dialog || NULL == dialog->session);
		r = t->oninvite(t->param, reply, t, dialog, reply->u.s.code, NULL); // ignore session
	}
	else
	{
		// 1. miss order / in-flight
		// 2. fork invite
	}

	// 17.1.1.3 Construction of the ACK Request
	// The ACK MUST be sent to the same address, port, 
	// and transport to which the original request was sent.
	sip_uac_ack_3456xx(t, reply, dialog); // ignore ack transport layer error, retry send on retransmissions

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

	sip_dialog_release(dialog);
	return r;
}

static int sip_uac_transaction_invite_accepted(struct sip_uac_transaction_t* t, const struct sip_message_t* reply, int retransmissions)
{
	int r;
	struct sip_dialog_t* dialog;
	r = 0;

	// TODO: add dialog locker here
	dialog = sip_dialog_fetch(t->agent, &reply->callid, &reply->from.tag, &reply->to.tag);

	// only create new dialog on first 2xx response
	if (!dialog && cstrvalid(&reply->to.tag) && !retransmissions)
	{
		dialog = sip_dialog_create();
		if (!dialog) return -1;
		if (0 != sip_dialog_init_uac(dialog, reply) || 0 != sip_dialog_set_local_target(dialog, t->req) || 0 != sip_dialog_add(t->agent, dialog))
		{
			sip_dialog_release(dialog);
			dialog = NULL;
			return 0; // ignore
		}
	}

	if (!dialog)
		return 0; // ignore fork response

    // update dialog target
    sip_dialog_target_refresh(dialog, reply);
	sip_dialog_addref(dialog); // for t->dialog
	t->dialog = dialog;
    
	if (dialog->state == DIALOG_ERALY)
	{
		assert(!retransmissions);
		t->status = SIP_UAC_TRANSACTION_ACCEPTED_UNACK;

		// transfer dialog state
		dialog->state = DIALOG_CONFIRMED;
		
		// completed To tag
        assert(cstrvalid(&dialog->remote.uri.tag));
        
		// receive and pass to the TU any retransmissions of the 2xx
		// response or any additional 2xx responses from other branches of a
		// downstream fork of the matching request.
		assert(200 <= reply->u.s.code && reply->u.s.code < 300);
		r = t->oninvite(t->param, reply, t, dialog, reply->u.s.code, &dialog->session);
	}

	// 17.1.1.3 Construction of the ACK Request ==> 13.2.2.4 2xx Responses 
	// RFC6026: An element encountering an unrecoverable transport error when trying
	// to send a response to an INVITE request MUST NOT immediately destroy
	// the associated INVITE server transaction state.  This state is
	// necessary to ensure correct processing of retransmissions of the request.

	// need user ack
	//sip_uac_ack(t, reply, dialog); // ignore ack transport layer error, retry send on retransmissions
	
	if (!retransmissions)
	{
		// If a 2xx response is received while the client INVITE state machine is 
		// in the "Calling" or "Proceeding" states, it MUST transition to the 
		// "Accepted" state, pass the 2xx response to the TU, and set Timer M to 64*T1.
		sip_uac_transaction_timewait(t, TIMER_M);
	}

	sip_dialog_release(dialog);
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
		else if (300 <= reply->u.s.code && reply->u.s.code < 700)
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

	case SIP_UAC_TRANSACTION_ACCEPTED_UNACK:
	case SIP_UAC_TRANSACTION_ACCEPTED_ACKED:
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
	
	// stop retry timer A/B
	sip_uac_stop_timer(t->agent, t, &t->timera);

	oldstatus = t->status;
	status = sip_uac_transaction_inivte_change_state(t, reply);

	r = 0;
	switch (status)
	{
	case SIP_UAC_TRANSACTION_CALLING:
	case SIP_UAC_TRANSACTION_PROCEEDING:
		r = sip_uac_transaction_invite_proceeding(t, reply);
		break;

	case SIP_UAC_TRANSACTION_ACCEPTED:
		if (200 <= reply->u.s.code && reply->u.s.code < 300)
			r = sip_uac_transaction_invite_accepted(t, reply, oldstatus == status);
		// ignore other status code (fork)
		break;

	case SIP_UAC_TRANSACTION_COMPLETED:
		// ignore other status code (fork or misorder)
		r = sip_uac_transaction_invite_completed(t, reply, oldstatus == status);
		
		// TODO: fork 2xx response
		//if (200 <= reply->u.s.code && reply->u.s.code < 300)
		break;

	case SIP_UAC_TRANSACTION_TERMINATED:
		break; // timeout ?

	case SIP_UAC_TRANSACTION_ACCEPTED_UNACK:
		// 200 OK -> wait for user ack(sip_uac_ack)
		// nothing to do
		break;

	case SIP_UAC_TRANSACTION_ACCEPTED_ACKED:
		// 200 OK -> ACK(retransmission)
		if(t->transport.send && t->size > 0)
			r = t->transport.send(t->transportptr, t->data, t->size);
		break;

	default:
		assert(0);
		break;
	}
	return r;
}
