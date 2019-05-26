#include "sip-uas-transaction.h"

// Figure 8: non-INVITE server transaction (p140)
int sip_uas_transaction_noninvite_input(struct sip_uas_transaction_t* t, struct sip_dialog_t* dialog, const struct sip_message_t* req)
{
	int r;
	switch (t->status)
	{
	case SIP_UAS_TRANSACTION_INIT:
		t->status = SIP_UAS_TRANSACTION_TRYING;
		r = sip_uas_transaction_handler(t, dialog, req);
		if (0 != r && SIP_UAS_TRANSACTION_TRYING == t->status)
		{
			// user ignore/discard
			t->status = SIP_UAS_TRANSACTION_TERMINATED;
			sip_uas_transaction_timewait(t, TIMER_H);
		}
		return r;

	case SIP_UAS_TRANSACTION_TRYING:
		// Once in the "Trying" state, any further request
		// retransmissions are discarded.
		return 0;

	case SIP_UAS_TRANSACTION_PROCEEDING:
		// If a retransmission of the request is received while in 
		// the "Proceeding" state, the most recently sent provisional 
		// response MUST be passed to the transport layer for retransmission.
		r = sip_uas_transaction_dosend(t);
		assert(0 == r); // ignore transport error(client will retransmission request)
		return 0;

	case SIP_UAS_TRANSACTION_COMPLETED:
		// 1. While in the "Completed" state, the server transaction MUST pass 
		//    the final response to the transport layer for retransmission 
		//    whenever a retransmission of the request is received.
		// 2. Any other final responses passed by the TU to the server
		//    transaction MUST be discarded while in the "Completed" state
		r = sip_uas_transaction_dosend(t);
		assert(0 == r); // ignore transport error(client will retransmission request)
		return 0;

	case SIP_UAS_TRANSACTION_TERMINATED:
		return 0;

	default:
		assert(0);
		return -1;
	}
}

int sip_uas_transaction_noninvite_reply(struct sip_uas_transaction_t* t, int code, const void* data, int bytes)
{
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
	else if (200 <= code && code < 700)
	{
		t->status = SIP_UAS_TRANSACTION_COMPLETED;
	}
	else
	{
		assert(0);
		return -1; // invalid code
	}

	if (SIP_UAS_TRANSACTION_COMPLETED == t->status)
	{
		// start timer J
		sip_uas_transaction_timewait(t, t->reliable ? 1 : TIMER_J);
	}

	return sip_uas_transaction_dosend(t);
}
