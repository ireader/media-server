#include "sip-uas-transaction.h"

int sip_uas_oncancel(struct sip_uas_transaction_t* t, struct sip_dialog_t* dialog, const struct sip_message_t* req)
{
	struct sip_uas_transaction_t* origin;

	// 487 Request Terminated
	// CANCEL has no effect on a request to which a UAS has already given a final response

	origin = sip_uas_find_transaction(t->agent, req, 0);
	if (!origin)
	{
		// 481 Call Leg/Transaction Does Not Exist
		return sip_uas_transaction_noninvite_reply(t, 481, NULL, 0);
	}

	if (origin->status > SIP_UAS_TRANSACTION_PROCEEDING)
	{
		// If it has already sent a final response for the original request, 
		// the CANCEL request has no effect on the processing of the original 
		// request, no effect on any session state, and no effect on the 
		// responses generated for the original request.

		return sip_uas_transaction_noninvite_reply(t, 200, NULL, 0);
	}
	
	// the To tag of the response to the CANCEL and the To tag
	// in the response to the original request SHOULD be the same.
	t->reply->ptr.ptr = cstring_clone(t->reply->ptr.ptr, t->reply->ptr.end, &t->reply->to.tag, origin->reply->to.tag.p, origin->reply->to.tag.n);
	
	return t->handler->oncancel(t->param, req, t, dialog ? dialog->session : NULL);
}
