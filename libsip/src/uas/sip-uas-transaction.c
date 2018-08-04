#include "sip-uas-transaction.h"

static int sip_uas_transaction_handler(struct sip_uas_transaction_t* t, struct sip_dialog_t* dialog, const struct sip_message_t* req)
{
	if (0 == cstrcasecmp(&req->u.c.method, "ACK"))
	{
	}
	else if (0 == cstrcasecmp(&req->u.c.method, "CANCEL"))
	{
	}
	else if (0 == cstrcasecmp(&req->u.c.method, "BYE"))
	{
	}
	else if (0 == cstrcasecmp(&req->u.c.method, "REGISTER"))
	{
		return t->uas->handler->onregister(t->uas->param, t, );
	}
	else if (0 == cstrcasecmp(&req->u.c.method, "OPTIONS"))
	{
		return sip_uas_transaction_noninvite_reply(t, 200, );
	}
	else
	{
		assert(0);
		return 405; // Method Not Allowed
	}
}

int sip_uas_transaction_noninvite_input(struct sip_uas_transaction_t* t, struct sip_dialog_t* dialog, const struct sip_message_t* req)
{
	int r;
	switch (t->status)
	{
	case SIP_UAS_TRANSACTION_INIT:
		t->status = SIP_UAS_TRANSACTION_TRYING;
		return sip_uas_transaction_handler(t, dialog, req);

	case SIP_UAS_TRANSACTION_TRYING:
		// Once in the "Trying" state, any further request
		// retransmissions are discarded.
		return 0;

	case SIP_UAS_TRANSACTION_PROCEEDING:
		// If a retransmission of the request is received while in 
		// the "Proceeding" state, the most recently sent provisional 
		// response MUST be passed to the transport layer for retransmission.
		assert(t->size > 0);
		r = t->uas->handler->send(t->uas->param, t->data, t->size);
		assert(0 == r); // ignore transport error(client will retransmission request)
		return 0;

	case SIP_UAS_TRANSACTION_COMPLETED:
		// 1. While in the "Completed" state, the server transaction MUST pass 
		//    the final response to the transport layer for retransmission 
		//    whenever a retransmission of the request is received.
		// 2. Any other final responses passed by the TU to the server
		//    transaction MUST be discarded while in the "Completed" state
		assert(t->size > 0);
		r = t->uas->handler->send(t->uas->param, t->data, t->size);
		assert(0 == r); // ignore transport error(client will retransmission request)
		return 0;

	case SIP_UAS_TRANSACTION_TERMINATED:
	default:
		assert(0);
		return -1;
	}
}

int sip_uas_transaction_noninvite_reply(struct sip_uas_transaction_t* t, int code, const void* data, int bytes)
{
	int r;
	assert(SIP_UAS_TRANSACTION_TRYING == t->status || SIP_UAS_TRANSACTION_PROCEEDING == t->status);
	if (SIP_UAS_TRANSACTION_TRYING != t->status && SIP_UAS_TRANSACTION_PROCEEDING != t->status)
		return 0; // discard

	t->msg->u.s.code = code;
	t->msg->u.s.reason.p = sip_reason_phrase(code);
	t->msg->u.s.reason.n = strlen(t->msg->u.s.reason.p);
	t->msg->payload = data;
	t->msg->size = bytes;

	t->size = t->msg->write(t->data);
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
		if (t->uas->transport->reliable(t->uas->transportptr))
		{
			// start timer J
			r = t->uas->timer.start(t->uas->timerptr, 64 * T1, sip_uas_transaction_terminated, t);
			if (0 != r) return r;
		}
	}
	else
	{
		assert(0);
		return -1; // invalid code
	}

	return t->uas->handler->send(t->uas->param, t->data, t->size);
}
