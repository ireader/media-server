#include "sip-uas-transaction.h"

int sip_uas_transaction_invite_input(struct sip_uas_transaction_t* t, struct sip_dialog_t* dialog, const struct sip_message_t* req)
{
	switch (t->status)
	{
	case SIP_UAS_TRANSACTION_TRYING:
		// reply trying
		t->uas->handler->send(t->uas->param, "TRYING");
		// notify user
		t->uas->handler->onmsg(t->uas->param, t, req);
		t->status = SIP_UAS_TRANSACTION_PROCEEDING;
		break;

	case SIP_UAS_TRANSACTION_PROCEEDING:
		// send last proceeding reply
		t->uas->handler->send(t->uas->param, "PROCEEDING");
		break;

	case SIP_UAS_TRANSACTION_COMPLETED:
	case SIP_UAS_TRANSACTION_COMFIRMED:
	case SIP_UAS_TRANSACTION_TERMINATED:
	default:
		assert(0);
	}
}

int sip_uas_transaction_invite_reply(struct sip_uas_transaction_t* t, int code, const void* data, int bytes)
{
	int r;
	assert(SIP_UAS_TRANSACTION_PROCEEDING == t->status);
	if (SIP_UAS_TRANSACTION_PROCEEDING != t->status)
		return 0; // discard

	if (100 <= code && code < 200)
	{
		// provisional response
	}
	else if (200 <= code && code < 700)
	{
		t->status = SIP_UAS_TRANSACTION_COMPLETED;
		if (t->uas->transport->reliable(t->uas->transportptr))
		{
			r = t->uas->timer.start(t->uas->timerptr, T1, sip_uas_transaction_retransmission, t);
			if (0 != r) return r;
		}
	}
	else
	{
		assert(0);
		return -1; // invalid code
	}

	t->msg->u.s.code = code;
	t->msg->u.s.reason.p = sip_reason_phrase(code);
	t->msg->u.s.reason.n = strlen(t->msg->u.s.reason.p);
	t->msg->payload = data;
	t->msg->size = bytes;

	n = t->msg->write(t->data);
	return t->uas->handler->send(t->uas->param, t->data, n);
}
