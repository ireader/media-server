#include "sip-uac.h"
#include "sip-uac-transaction.h"
#include "sip-timer.h"
#include "sip-header.h"
#include "sip-dialog.h"
#include "sip-message.h"
#include "sip-transport.h"

#include "sys/atomic.h"
#include "sys/locker.h"
#include "cstringext.h"
#include "list.h"

struct sip_uac_t
{
	int32_t ref;
	locker_t locker;

	struct sip_timer_t timer;
	void* timerptr;

	struct list_head transactions; // transaction layer handler
	struct list_head dialogs; // early or confirmed dialogs
};

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
		sip_uac_transaction_release(t);
	}

	list_for_each_safe(pos, next, &uac->dialogs)
	{
		dialog = list_entry(pos, struct sip_dialog_t, link);
		sip_dialog_release(dialog);
	}

	locker_destroy(&uac->locker);
	free(uac);
	return 0;
}

int sip_uac_add_dialog(struct sip_uac_t* uac, struct sip_dialog_t* dialog)
{
	// link to tail
	assert(uac->ref > 0);
	locker_lock(&uac->locker);
	list_insert_after(&dialog->link, uac->dialogs.prev);
	locker_unlock(&uac->locker);
	return sip_dialog_addref(dialog);
}

int sip_uac_del_dialog(struct sip_uac_t* uac, struct sip_dialog_t* dialog)
{
	// unlink dialog
	assert(uac->ref > 0);
	locker_lock(&uac->locker);
	list_remove(&dialog->link);
	locker_unlock(&uac->locker);
	return sip_dialog_release(dialog);
}

struct sip_dialog_t* sip_uac_find_dialog(struct sip_uac_t* uac, const struct sip_message_t* msg)
{
	struct list_head *pos, *next;
	struct sip_dialog_t* dialog;

	list_for_each_safe(pos, next, &uac->dialogs)
	{
		dialog = list_entry(pos, struct sip_dialog_t, link);
		if (sip_dialog_match(dialog, &msg->callid, &msg->from.tag, &msg->to.tag))
			return dialog;
	}
	return NULL;
}

int sip_uac_add_transaction(struct sip_uac_t* uac, struct sip_uac_transaction_t* t)
{
	// link to tail
	assert(uac->ref > 0);
	locker_lock(&uac->locker);
	list_insert_after(&t->link, uac->transactions.prev);
	locker_unlock(&uac->locker);
	return sip_uac_transaction_addref(t);
}

int sip_uac_del_transaction(struct sip_uac_t* uac, struct sip_uac_transaction_t* t)
{
	struct sip_dialog_t* dialog;
	struct list_head *pos, *next;

	assert(uac->ref > 0);
	locker_lock(&uac->locker);

	// unlink transaction
	list_remove(&t->link);

	list_for_each_safe(pos, next, &uac->dialogs)
	{
		dialog = list_entry(pos, struct sip_dialog_t, link);
		if (0 == cstrcmp(&t->req->callid, dialog->callid) && DIALOG_ERALY == dialog->state)
		{
			list_remove(pos);
			sip_dialog_release(dialog); // WARNING: release in locker
		}
	}

	locker_unlock(&uac->locker);
	return sip_uac_transaction_release(t);
}

void* sip_uac_start_timer(struct sip_uac_t* uac, int timeout, sip_timer_handle handler, void* usrptr)
{
	return uac->timer.start(uac->timerptr, timeout, handler, usrptr);
}

void sip_uac_stop_timer(struct sip_uac_t* uac, void* id)
{
	uac->timer.stop(uac->timerptr, id);
}

// RFC3261 17.1.3 Matching Responses to Client Transactions (p132)
static struct sip_uac_transaction_t* sip_uac_find_transaction(struct list_head* transactions, struct sip_message_t* reply)
{
	const struct cstring_t *p, *p2;
	struct list_head *pos, *next;
	struct sip_uac_transaction_t* t;

	p = sip_vias_top_branch(&reply->vias);
	if (!p) return NULL;
	assert(0 == cstrprefix(p, SIP_BRANCH_PREFIX));

	list_for_each_safe(pos, next, transactions)
	{
		t = list_entry(pos, struct sip_uac_transaction_t, link);

		// 1. via branch parameter
		p2 = sip_vias_top_branch(&t->req->vias);
		if (!p2 || 0 == cstreq(p, p2))
			continue;
		assert(0 == cstrprefix(p2, SIP_BRANCH_PREFIX));
		
		// 2. cseq method parameter
		// The method is needed since a CANCEL request constitutes a
		// different transaction, but shares the same value of the branch parameter.
		assert(reply->cseq.id == t->req->cseq.id);
		if (!cstreq(&reply->cseq.method, &t->req->cseq.method))
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

int sip_uac_send(struct sip_uac_transaction_t* t, const void* sdp, int bytes, struct sip_transport_t* transport, void* param)
{
	int r;
	char via[256];
	char local[128];
	char remote[256]; // destination/router
	char protocol[16];

	// router
	r = sip_message_get_next_hop(t->req, remote, sizeof(remote));
	if (0 != r)
		return r;

	r = transport->via(param, remote, protocol, local);
	if (0 != r)
		return r;

	// Via: SIP/2.0/UDP erlang.bell-telephone.com:5060;branch=z9hG4bK87asdks7
	// Via: SIP/2.0/UDP first.example.com:4000;ttl=16;maddr=224.2.0.1;branch=z9hG4bKa7c6a8dlze.1
	r = snprintf(via, sizeof(via), "SIP/2.0/%s %s;branch=%s%p", protocol, local, SIP_BRANCH_PREFIX, t);
	if (r < 0 || r >= sizeof(via))
		return -1; // ENOMEM
	sip_message_add_header(t->req, "Via", via);

	// message
	t->req->payload = sdp;
	t->req->size = bytes;
	t->size = sip_message_write(t->req, t->data, sizeof(t->data));
	if (t->size < 0 || t->size >= sizeof(t->data))
	{
		sip_uac_transaction_release(t);
		return -1;
	}

	atomic_increment32(&t->uac->ref); // ref by transaction
	t->transport = transport;
	t->transportptr = param;
	return sip_uac_transaction_send(t);
}

int sip_uac_add_header(struct sip_uac_transaction_t* t, const char* name, const char* value)
{
	return sip_message_add_header(t->req, name, value);
}

int sip_uac_add_header_int(struct sip_uac_transaction_t* t, const char* name, int value)
{
	char v[32];
	snprintf(v, sizeof(v), "%d", value);
	return sip_uac_add_header(t, name, v);
}
