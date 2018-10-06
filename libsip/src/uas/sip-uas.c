#include "sip-uas.h"
#include "sip-uas-transaction.h"
#include "sip-timer.h"
#include "sip-header.h"
#include "sip-dialog.h"
#include "sip-message.h"
#include "sip-transport.h"
#include "sys/atomic.h"
#include "sys/locker.h"
#include "list.h"
#include <stdio.h>

struct sip_uas_t
{
	int32_t ref;
	locker_t locker;

	//struct sip_timer_t timer;
	//void* timerptr;

	struct list_head transactions; // transaction layer handler
	struct list_head dialogs; // early or confirmed dialogs

	struct sip_uas_handler_t handler;
	void* param;
};

struct sip_uas_t* sip_uas_create(const char* name, struct sip_uas_handler_t* handler, void* param)
{
	struct sip_uas_t* uas;
	uas = (struct sip_uas_t*)calloc(1, sizeof(*uas));
	if (NULL == uas)
		return NULL;

	uas->ref = 1;
	locker_create(&uas->locker);
	LIST_INIT_HEAD(&uas->dialogs);
	LIST_INIT_HEAD(&uas->transactions);
	memcpy(&uas->handler, handler, sizeof(uas->handler));
	uas->param = param;
	return uas;
}

int sip_uas_destroy(struct sip_uas_t* uas)
{
	struct sip_dialog_t* dialog;
	struct list_head *pos, *next;
	struct sip_uas_transaction_t* t;

	assert(uas->ref > 0);
	if (0 != atomic_decrement32(&uas->ref))
		return 0;

	list_for_each_safe(pos, next, &uas->transactions)
	{
		t = list_entry(pos, struct sip_uas_transaction_t, link);
		assert(t->uas == uas);
		sip_uas_transaction_release(t);
	}

	list_for_each_safe(pos, next, &uas->dialogs)
	{
		dialog = list_entry(pos, struct sip_dialog_t, link);
		sip_dialog_release(dialog);
	}

	locker_destroy(&uas->locker);
	free(uas);
	return 0;
}

int sip_uas_add_dialog(struct sip_uas_t* uas, struct sip_dialog_t* dialog)
{
	// link to tail
	assert(uas->ref > 0);
	locker_lock(&uas->locker);
	list_insert_after(&dialog->link, uas->dialogs.prev);
	locker_unlock(&uas->locker);
	return sip_dialog_addref(dialog);
}

int sip_uas_del_dialog(struct sip_uas_t* uas, struct sip_dialog_t* dialog)
{
	// unlink dialog
	assert(uas->ref > 0);
	locker_lock(&uas->locker);
	list_remove(&dialog->link);
	locker_unlock(&uas->locker);
	return sip_dialog_release(dialog);
}

void* sip_uas_start_timer(struct sip_uas_t* uas, int timeout, sip_timer_handle handler, void* usrptr)
{
	//return uas->timer.start(uas->timerptr, timeout, handler, usrptr);
	return sip_timer_start(timeout, handler, usrptr);
}

void sip_uas_stop_timer(struct sip_uas_t* uas, void* id)
{
	//uas->timer.stop(uas->timerptr, id);
	sip_timer_stop(id);
}

int sip_uas_add_transaction(struct sip_uas_t* uas, struct sip_uas_transaction_t* t)
{
	t->param = uas->param;
	t->handler = &uas->handler;

	// link to tail
	assert(uas->ref > 0);
	locker_lock(&uas->locker);
	list_insert_after(&t->link, uas->transactions.prev);
	locker_unlock(&uas->locker);
	return sip_uas_transaction_addref(t);
}

int sip_uas_del_transaction(struct sip_uas_t* uas, struct sip_uas_transaction_t* t)
{
	struct sip_dialog_t* dialog;
	struct list_head *pos, *next;

	assert(uas->ref > 0);
	locker_lock(&uas->locker);

	// unlink transaction
	list_remove(&t->link);

	// 12.3 Termination of a Dialog (p77)
	// Independent of the method, if a request outside of a dialog generates
	// a non-2xx final response, any early dialogs created through
	// provisional responses to that request are terminated.
	list_for_each_safe(pos, next, &uas->dialogs)
	{
		dialog = list_entry(pos, struct sip_dialog_t, link);
		if (0 == cstrcmp(&t->reply->callid, dialog->callid) && DIALOG_ERALY == dialog->state)
		{
			list_remove(pos);
			sip_dialog_release(dialog); // WARNING: release in locker
		}
	}

	locker_unlock(&uas->locker);
	return sip_uas_transaction_release(t);
}

static struct sip_uas_transaction_t* sip_uas_find_acktransaction(struct sip_uas_t* uas, const struct sip_message_t* req)
{
	struct list_head *pos, *next;
	struct sip_uas_transaction_t* t;

	list_for_each_safe(pos, next, &uas->transactions)
	{
		t = list_entry(pos, struct sip_uas_transaction_t, link);
		if (cstreq(&t->reply->callid, &req->callid) && cstreq(&t->reply->from.tag, &req->from.tag) && cstreq(&t->reply->to.tag, &req->to.tag))
			return t;
	}

	return NULL;
}

// RFC3261 17.2.3 Matching Requests to Server Transactions (p138)
struct sip_uas_transaction_t* sip_uas_find_transaction(struct sip_uas_t* uas, const struct sip_message_t* req, int matchmethod)
{
	struct list_head *pos, *next;
	struct sip_uas_transaction_t* t;
	const struct sip_via_t *via, *via2;

	via = sip_vias_get(&req->vias, 0);
	if (!via) return NULL; // invalid sip message
	assert(cstrprefix(&via->branch, SIP_BRANCH_PREFIX));

	list_for_each_safe(pos, next, &uas->transactions)
	{
		t = list_entry(pos, struct sip_uas_transaction_t, link);
		via2 = sip_vias_get(&t->reply->vias, 0);
		assert(via2);

		// 1. via branch parameter
		if (!cstreq(&via->branch, &via2->branch))
			continue;
		assert(cstrprefix(&via2->branch, SIP_BRANCH_PREFIX));

		// 2. via send-by value
		// The sent-by value is used as part of the matching process because
		// there could be accidental or malicious duplication of branch
		// parameters from different clients.
		if(!cstreq(&via->host, &via2->host))
			continue;

		// 3. cseq method parameter
		// the method of the request matches the one that created the
		// transaction, except for ACK, where the method of the request
		// that created the transaction is INVITE
		assert(req->cseq.id == t->reply->cseq.id);
		assert(cstreq(&req->cseq.method, &t->reply->cseq.method) || 0 == cstrcasecmp(&req->cseq.method, SIP_METHOD_CANCEL) || 0 == cstrcasecmp(&req->cseq.method, SIP_METHOD_ACK));
		if (matchmethod && cstreq(&req->cseq.method, &t->reply->cseq.method))
			return t;

		// ACK/CANCEL find origin request transaction
		if (!matchmethod && !cstreq(&req->cseq.method, &t->reply->cseq.method) && 0 != cstrcasecmp(&t->reply->cseq.method, SIP_METHOD_CANCEL) && 0 != cstrcasecmp(&t->reply->cseq.method, SIP_METHOD_ACK))
			return t;
	}

	// The ACK for a 2xx response to an INVITE request is a separate transaction
	return sip_message_isack(req) ? sip_uas_find_acktransaction(uas, req) : NULL;
}

static struct sip_dialog_t* sip_uas_find_dialog(struct sip_uas_t* uas, const struct sip_message_t* msg)
{
	struct list_head *pos, *next;
	struct sip_dialog_t* dialog;

	list_for_each_safe(pos, next, &uas->dialogs)
	{
		dialog = list_entry(pos, struct sip_dialog_t, link);
		if (sip_dialog_match(dialog, &msg->callid, &msg->to.tag, &msg->from.tag))
			return dialog;
	}
	return NULL;
}

static struct sip_dialog_t* sip_uas_create_dialog(struct sip_uas_transaction_t* t, const struct sip_message_t* req)
{
	struct sip_dialog_t* dialog;
	struct sip_message_t msg;
	memcpy(&msg, req, sizeof(msg));
	memcpy(&msg.to, &req->from, sizeof(req->from));
	memcpy(&msg.from, &t->reply->to, sizeof(t->reply->to)); // create with to.tag
	dialog = sip_dialog_create(&msg);
	if (dialog)
	{
		dialog->local.id = dialog->remote.id;
		dialog->remote.id = req->cseq.id;
	}
	return dialog;
}

int sip_uas_input(struct sip_uas_t* uas, const struct sip_message_t* msg)
{
	struct sip_dialog_t *dialog;
	struct sip_uas_transaction_t* t;

	// 1. find transaction
	t = sip_uas_find_transaction(uas, msg, 1);
	if (!t)
	{
		if (sip_message_isack(msg))
			return 0; // invalid ack, discard, TODO: add log here

		t = sip_uas_transaction_create(uas, msg);
		if (!t) return -1;
	}

	// 2. find dialog
	dialog = sip_uas_find_dialog(uas, msg);
	if (!dialog && sip_message_isinvite(msg))
	{
		// 3. create early dialog
		dialog = sip_uas_create_dialog(t, msg);
		if (!dialog)
		{
			sip_uas_transaction_release(t);
			return -1;
		}
		sip_uas_add_dialog(t->uas, dialog);
	}

	// 4. handle
	if (sip_message_isinvite(msg) || sip_message_isack(msg))
	{
		return sip_uas_transaction_invite_input(t, dialog, msg);
	}
	else
	{
		return sip_uas_transaction_noninvite_input(t, dialog, msg);
	}

	// TODO:
	// 1. A stateless UAS MUST NOT send provisional (1xx) responses.
	// 2. A stateless UAS MUST NOT retransmit responses.
	// 3. A stateless UAS MUST ignore ACK requests.
	// 4. A stateless UAS MUST ignore CANCEL requests.
}

int sip_uas_reply(struct sip_uas_transaction_t* t, int code, const void* data, int bytes)
{
	if (sip_message_isinvite(t->reply))
	{
		return sip_uas_transaction_invite_reply(t, code, data, bytes);
	}
	else
	{
		return sip_uas_transaction_noninvite_reply(t, code, data, bytes);
	}
}

int sip_uas_discard(struct sip_uas_transaction_t* t)
{
	return sip_uas_transaction_release(t);
}

int sip_uas_add_header(struct sip_uas_transaction_t* t, const char* name, const char* value)
{
	return sip_message_add_header(t->reply, name, value);
}

int sip_uas_add_header_int(struct sip_uas_transaction_t* t, const char* name, int value)
{
	return sip_message_add_header_int(t->reply, name, value);
}
