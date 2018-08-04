#include "sip-uas.h"
#include "sip-message.h"
#include "sip-uas-transaction.h"

static struct sip_contact_t* sip_uas_from(const char* name)
{
	struct sip_contact_t from;
	memset(&from, 0, sizeof(from));
	return 0 == sip_header_contact(name, name + strlen(name), &from) ? &from : NULL;
}

struct sip_uas_t* sip_uas_create(const char* name, struct sip_uas_handler_t* handler, void* param)
{
	struct sip_uas_t* uas;
	struct sip_contact_t* from;

	uas = (struct sip_uas_t*)calloc(1, sizeof(*uas));
	if (NULL == uas)
		return NULL;

	if ((from = sip_uas_from(name), !from) || 0 != sip_contact_clone(&uas->user, from))
	{
		sip_uas_destroy(uas);
		return NULL;
	}

	uas->ref = 1;
	locker_create(&uas->locker);
	LIST_INIT_HEAD(&uas->dialogs);
	LIST_INIT_HEAD(&uas->transactions);
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
		sip_uas_transaction_destroy(t);
	}

	list_for_each_safe(pos, next, &uas->dialogs)
	{
		dialog = list_entry(pos, struct sip_dialog_t, link);
		sip_dialog_destroy(dialog);
	}

	locker_destroy(&uas->locker);
	free(uas);
	return 0;
}

// RFC3261 17.2.3 Matching Requests to Server Transactions (p138)
static struct sip_uas_transaction_t* sip_uas_find_transaction(struct list_head* transactions, struct sip_message_t* req)
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
		if(!cstreq(&via->host, &via2->host))
			continue;

		// 3. cseq method parameter
		// the method of the request matches the one that created the
		// transaction, except for ACK, where the method of the request
		// that created the transaction is INVITE
		assert(req->cseq.id == t->msg->cseq.id);
		if (!cstreq(&req->cseq.method, &t->msg->cseq.method) && 0 != cstrcasecmp(&req->cseq.method, "ACK"))
			continue;

		return t;
	}

	return NULL;
}

static struct sip_dialog_t* sip_uas_find_dialog(struct sip_uas_t* uas, const struct sip_message_t* msg)
{
	struct list_head *pos, *next;
	struct sip_dialog_t* dialog;

	list_for_each_safe(pos, next, &uas->dialogs)
	{
		dialog = list_entry(pos, struct sip_dialog_t, link);
		if (sip_dialog_match(dialog, &msg->callid, &msg->from.tag, &msg->to.tag))
			return dialog;
	}
	return NULL;
}

int sip_uas_input(struct sip_uas_t* uas, struct http_parser_t* parser)
{
	struct sip_message_t msg;
	struct sip_dialog_t *dialog;
	struct sip_uas_transaction_t* t;
	sip_message_load(parser, &msg);

	// 1. find transaction
	t = sip_uas_find_transaction(&uas->transactions, &msg);
	if (!t)
	{
		t = sip_uas_transaction_create(uas, &msg);
		if (!t) return -1;
	}

	// 2. find dialog
	dialog = sip_uas_find_dialog(&uas->dialogs, &msg);

	// 3. handle
	if (sip_message_isinvite(&msg))
	{
		return sip_uas_transaction_invite_input(t, dialog, &msg);
	}
	else
	{
		return sip_uas_transaction_noninvite_input(t, dialog, &msg);
	}
}

int sip_uas_reply(struct sip_uas_transaction_t* t, int code, const void* data, int bytes)
{
	if (sip_message_isinvite(&t->msg))
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
}

int sip_uas_get_header_count(struct sip_uas_transaction_t* t)
{
	return http_get_header_count(t->http);
}

int sip_uas_get_header(struct sip_uas_transaction_t* t, int i, const char** name, const char** value)
{
	return http_get_header(t->http, i, name, value);
}

const char* sip_uas_get_header_by_name(struct sip_uas_transaction_t* t, const char* name)
{
	return http_get_header_by_name(t->http, name);
}
