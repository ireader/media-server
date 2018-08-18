#include "sip-uas-transaction.h"

static struct sip_uas_transaction_t* sip_uas_find_origin(struct list_head* transactions, struct sip_message_t* req)
{
	struct list_head *pos, *next;
	struct sip_uas_transaction_t* t;
	const struct sip_via_t *via, *via2;

	via = sip_vias_get(&req->vias, 0);
	if (!via) return -1; // invalid sip message
	assert(0 == cstrprefix(&via->branch, SIP_BRANCH_PREFIX));

	list_for_each_safe(pos, next, transactions)
	{
		t = list_entry(pos, struct sip_uas_transaction_t, link);
		via2 = sip_vias_get(&t->msg->vias, 0);
		assert(via2);

		// 1. via branch parameter
		if (!cstreq(&via->branch, &via2->branch))
			continue;
		assert(0 == cstrprefix(&via2->branch, SIP_BRANCH_PREFIX));

		// 2. via send-by value
		// The sent-by value is used as part of the matching process because
		// there could be accidental or malicious duplication of branch
		// parameters from different clients.
		if (!cstreq(&via->host, &via2->host))
			continue;

		// 3. cseq method parameter
		assert(req->cseq.id == t->msg->cseq.id);
		if (0 == cstrcasecmp(&t->msg->cseq.method, "CANCEL") || 0 == cstrcasecmp(&req->cseq.method, "ACK"))
			continue;

		return t;
	}

	return NULL;
}

int sip_uas_oncancel(struct sip_uas_transaction_t* t, const struct sip_message_t* req)
{
	struct sip_uas_transaction_t* origin;

	// 487 Request Terminated
	// CANCEL has no effect on a request to which a UAS has already given a final response

	origin = sip_uas_find_origin(&t->uas->transactions, req);
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
	return t->uas->handler->oncancel(t->uas->param, t);
}
