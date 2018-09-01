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
		via2 = sip_vias_get(&t->req->vias, 0);
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
		assert(req->cseq.id == t->req->cseq.id);
		if (!cstreq(&req->cseq.method, &t->req->cseq.method) && 0 != cstrcasecmp(&req->cseq.method, "ACK"))
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

int sip_uas_input(struct sip_uas_t* uas, const struct sip_message_t* msg)
{
	struct sip_dialog_t *dialog;
	struct sip_uas_transaction_t* t;

	// 1. find transaction
	t = sip_uas_find_transaction(&uas->transactions, msg);
	if (!t)
	{
		t = sip_uas_transaction_create(uas, msg);
		if (!t) return -1;
	}
	t->req = msg;

	// 2. find dialog
	dialog = sip_uas_find_dialog(&uas->dialogs, msg);

	// 3. handle
	if (sip_message_isinvite(msg))
	{
		return sip_uas_transaction_invite_input(t, dialog, msg);
	}
	else
	{
		return sip_uas_transaction_noninvite_input(t, dialog, msg);
	}
}

int sip_uas_reply(struct sip_uas_transaction_t* t, int code, const void* data, int bytes)
{
	if (sip_message_isinvite(&t->reply))
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
	return sip_params_count(&t->req->headers);
}

int sip_uas_get_header(struct sip_uas_transaction_t* t, int i, struct cstring_t* const name, struct cstring_t* const value)
{
	struct sip_param_t* param;
	param = sip_params_get(&t->req->headers, i);
	if (!param) return -1;
	memcpy(&name, &param->name, sizeof(struct cstring_t));
	memcpy(&value, &param->value, sizeof(struct cstring_t));
	return 0;
}

const struct cstring_t* sip_uas_get_header_by_name(struct sip_uas_transaction_t* t, const char* name)
{
	return sip_params_find_string(&t->req->headers, name, strlen(name));
}

int sip_uas_add_header(struct sip_uas_transaction_t* t, const char* name, const char* value)
{
	int r;
	const char* end;
	struct sip_uri_t uri;
	struct sip_param_t header;

	// TODO: release memory
	value = strdup(value ? value : "");
	end = value + strlen(value);

	if (0 == strcasecmp(SIP_HEADER_FROM, name))
	{
		r = sip_header_contact(value, end, &t->reply->from);
	}
	else if (0 == strcasecmp(SIP_HEADER_TO, name))
	{
		r = sip_header_contact(value, end, &t->reply->to);
	}
	else if (0 == strcasecmp(SIP_HEADER_CALLID, name))
	{
		t->reply->callid.p = value;
		t->reply->callid.n = end - value;
	}
	else if (0 == strcasecmp(SIP_HEADER_CSEQ, name))
	{
		r = sip_header_cseq(value, end, &t->reply->cseq);
	}
	else if (0 == strcasecmp(SIP_HEADER_MAX_FORWARDS, name))
	{
		t->reply->maxforwards = strtol(value, NULL, 10);
	}
	else if (0 == strcasecmp(SIP_HEADER_VIA, name))
	{
		r = sip_header_vias(value, end, &t->reply->vias);
	}
	else if (0 == strcasecmp(SIP_HEADER_CONTACT, name))
	{
		r = sip_header_contacts(value, end, &t->reply->contacts);
	}
	else if (0 == strcasecmp(SIP_HEADER_ROUTE, name))
	{
		memset(&uri, 0, sizeof(uri));
		r = sip_header_uri(value, end, &uri);
		if (0 == r)
			sip_uris_push(&t->reply->routers, &uri);
	}
	else if (0 == strcasecmp(SIP_HEADER_RECORD_ROUTE, name))
	{
		memset(&uri, 0, sizeof(uri));
		r = sip_header_uri(value, end, &uri);
		if (0 == r)
			sip_uris_push(&t->reply->record_routers, &uri);
	}
	else
	{
		header.name.p = name;
		header.name.n = strlen(name);
		header.value.p = value ? value : "";
		header.value.n = value ? strlen(value) : 0;
		return sip_params_push(&t->reply->headers, &header);
	}
}

int sip_uas_add_header_int(struct sip_uas_transaction_t* t, const char* name, int value)
{
	char v[32];
	snprintf(v, sizeof(v), "%d", value);
	return sip_uas_add_header(t, name, v);
}
