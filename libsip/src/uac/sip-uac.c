#include "sip-uac.h"
#include "sip-uac-transaction.h"

static struct sip_contact_t* sip_uac_from(const char* name)
{
	struct sip_contact_t from;
	memset(&from, 0, sizeof(from));
	return 0 == sip_header_contact(name, name + strlen(name), &from) ? &from : NULL;
}

struct sip_uac_t* sip_uac_create(const char* name)
{
	struct sip_uac_t* uac;
	struct sip_contact_t* from;
	
	uac = (struct sip_uac_t*)calloc(1, sizeof(*uac));
	if (NULL == uac)
		return NULL;

	if ((from = sip_uac_from(name), !from) || 0 != sip_contact_clone(&uac->user, from))
	{
		sip_uac_destroy(uac);
		return NULL;
	}

	uac->ref = 1;
	locker_create(&uac->locker);
	LIST_INIT_HEAD(&uac->dialogs);
	LIST_INIT_HEAD(&uac->transactions);
	return uac;
}

int sip_uac_destroy(struct sip_uac_t* uac)
{
	struct sip_dialog_t* dialog;
	struct list_head *pos, *next;
	struct sip_uac_transaction_t* t;

	assert(uac->ref > 0);
	if (0 != atomic_decrement32(&uac->ref))
		return 0;

	list_for_each_safe(pos, next, &uac->transactions)
	{
		t = list_entry(pos, struct sip_uac_transaction_t, link);
		assert(t->uac == uac);
		sip_uac_transaction_destroy(t);
	}

	list_for_each_safe(pos, next, &uac->dialogs)
	{
		dialog = list_entry(pos, struct sip_dialog_t, link);
		sip_dialog_destroy(dialog);
	}

	locker_destroy(&uac->locker);
	free(uac);
	return 0;
}

// RFC3261 17.1.3 Matching Responses to Client Transactions (p132)
static struct sip_uac_transaction_t* sip_uac_find_transaction(struct list_head* transactions, struct sip_message_t* reply)
{
	struct cstring_t *p, *p2;
	struct list_head *pos, *next;
	struct sip_uac_transaction_t* t;

	p = sip_vias_top_branch(&reply->vias);
	if (!p) return NULL;
	assert(0 == cstrprefix(p, SIP_BRANCH_PREFIX));

	list_for_each_safe(pos, next, transactions)
	{
		t = list_entry(pos, struct sip_uac_transaction_t, link);

		// 1. via branch parameter
		p2 = sip_vias_top_branch(&t->msg->vias);
		if (!p2 || 0 == cstreq(p, p2))
			continue;
		assert(0 == cstrprefix(p2, SIP_BRANCH_PREFIX));
		
		// 2. cseq method parameter
		// The method is needed since a CANCEL request constitutes a
		// different transaction, but shares the same value of the branch parameter.
		assert(reply->cseq.id == t->msg->cseq.id);
		if (!cstreq(&reply->cseq.method, &t->msg->cseq.method))
			continue;

		//// 3. to tag
		//p = sip_params_find_string(&reply->to.params, "tag");
		//p2 = sip_params_find_string(&t->msg->to.params, "tag");
		//if (p2 && (!p || !cstreq(p, p2)))
		//	continue;

		return t;
	}

	return NULL;
}

int sip_uac_input(struct sip_uac_t* uac, struct sip_message_t* reply)
{
	struct sip_uac_transaction_t* t;

	// 1. find transaction
	t = sip_uac_find_transaction(&uac->transactions, reply);
	if (!t)
	{
		// timeout response, discard
		return 0;
	}

	if (sip_message_isinvite(reply))
	{
		return sip_uac_transaction_invite_input(t, reply);
	}
	else
	{
		return sip_uac_transaction_noninvite_input(t, reply);
	}
}

struct sip_uac_transaction_t* sip_uac_invite(struct sip_uac_t* uac, const char* to, const char* sdp, sip_uac_oninvite* oninvite, void* param)
{
	struct sip_message_t* msg;
	struct sip_uac_transaction_t* t;
	msg = sip_message_create(uac->name);
	t = sip_uac_transaction_create(uac, msg);
	sip_uac_transaction_send(t);
	return t;
}

int sip_uac_ack(struct sip_uac_t* uac, struct sip_dialog_t* dialog, const char* sdp)
{
	struct sip_message_t* msg;
	struct sip_uac_transaction_t* t;
	t = sip_uac_transaction_create(uac, msg);
	sip_uac_transaction_send(t);
	return t;
}

int sip_uac_bye(struct sip_uac_t* uac, struct sip_dialog_t* dialog)
{
	struct sip_message_t* msg;
	struct sip_uac_transaction_t* t;
	t = sip_uac_transaction_create(uac, msg);
	sip_uac_transaction_send(t);
	return t;
}

int sip_uac_cancel(struct sip_uac_t* uac, struct sip_uac_transaction_t* t)
{
	struct sip_message_t* msg;
	struct sip_uac_transaction_t* t;
	t = sip_uac_transaction_create(uac, msg);
	sip_uac_transaction_send(t);
	return t;
}
