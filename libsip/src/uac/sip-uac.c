#include "sip-uac.h"

struct sip_uac_t* sip_uac_create()
{
	struct sip_uac_t* uac;
	uac = (struct sip_uac_t*)calloc(1, sizeof(*uac));
	if (NULL == uac)
		return NULL;

	uac->ref = 1;
	locker_create(&uac->locker);
	LIST_INIT_HEAD(&uac->dialogs);
	LIST_INIT_HEAD(&uac->transactions);
	return uac;
}

int sip_uac_destroy(struct sip_uac_t* uac)
{
	struct list_head *pos, *next;
	assert(uac->ref > 0);
	if (0 != atomic_decrement32(&uac->ref))
		return 0;

	list_for_each_safe(pos, next, &uac->transactions)
	{
		struct sip_uac_transaction_t* t;
		t = list_entry(pos, struct sip_uac_transaction_t, link);
		assert(t->uac == uac);
		sip_uac_transaction_destroy(t);
	}

	list_for_each_safe(pos, next, &uac->dialogs)
	{
		struct sip_dialog_t* t;
		t = list_entry(pos, struct sip_dialog_t, link);
		// TODO
	}

	locker_destroy(&uac->locker);
	free(uac);
	return 0;
}

struct sip_uac_transaction_t* sip_uac_transaction_create(struct sip_uac_t* uac, const struct sip_message_t* msg)
{
	struct sip_uac_transaction_t* t;
	t = (struct sip_uac_transaction_t*)calloc(1, sizeof(*t));
	if (NULL == t) return NULL;

	atomic_increment32(&uac->ref);
	locker_create(&t->locker);
	t->msg = msg;
	t->uac = uac;
	t->status = SIP_UAC_TRANSACTION_TERMINATED;

	// link to tail
	locker_lock(&uac->locker);
	list_insert_after(&t->link, uac->transactions.prev);
	locker_unlock(&uac->locker);

	// message
	t->size = sip_message_write(msg, t->data, sizeof(t->data));
	if (t->size < 0 || t->size >= sizeof(t->data))
	{
		sip_uac_transaction_destroy(uac, t);
		return NULL;
	}
	return t;
}

int sip_uac_transaction_destroy(struct sip_uac_transaction_t* t)
{
	// unlink from uac
	locker_lock(&t->uac->locker);
	list_remove(&t->link);
	locker_unlock(&t->uac->locker);
	sip_uac_destroy(t->uac);

	locker_destroy(&t->locker);
	free(t);
	return 0;
}

// RFC3261 17.1.3 Matching Responses to Client Transactions (p132)
static struct sip_uac_transaction_t* sip_uac_transaction_find(struct list_head* transactions, struct sip_message_t* reply)
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
		if(reply->cseq.id != t->msg->cseq.id || !cstreq(&reply->cseq.method, &t->msg->cseq.method))
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
	t = sip_uac_transaction_find(&uac->transactions, reply);
	if (!t)
	{
		// timeout response, discard
		return 0;
	}
	
	if (cstrcasecmp(&reply->cseq.method, "INVITE"))
	{
		return sip_uac_transaction_invite_input(t, reply);
	}
	else
	{
		return sip_uac_transaction_noninvite_input(t, reply);
	}

	// 2. not find transaction, do it by UAC Core
	if (!invite)
	{
		// non-invite response, discard
		return 0;
	}

	// invite fork response
	t = sip_uac_transaction_find(&uac->invites, reply);
	if (t)
		return sip_uac_transaction_invite_input(t, reply);

	return 0; // not found, discard
}
