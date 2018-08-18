#include "sip-uac.h"
#include "sip-uac-transaction.h"

#include "sip-timer.h"
#include "sip-header.h"
#include "sip-dialog.h"
#include "sip-message.h"
#include "sip-transport.h"
#include "sys/atomic.h"
#include "sys/locker.h"
#include "list.h"

struct sip_uac_t
{
	int32_t ref;
	locker_t locker;
	struct sip_contact_t user; // sip from

	struct sip_transport_t* transport;
	void* transportptr;

	struct sip_timer_t timer;
	void* timerptr;

	struct list_head transactions; // transaction layer handler
	struct list_head dialogs; // early or confirmed dialogs
};

static struct sip_contact_t* sip_uac_from(const char* name)
{
	struct sip_contact_t from;
	memset(&from, 0, sizeof(from));
	return 0 == sip_header_contact(name, name + strlen(name), &from) ? &from : NULL;
}

struct sip_uac_t* sip_uac_create()
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

int sip_uac_send(struct sip_uac_transaction_t* t, const void* sdp, int bytes, sip_uac_onsend onsend, void* param)
{
	atomic_increment32(t->uac->ref); // ref by transaction

	// link to tail
	locker_lock(&t->uac->locker);
	list_insert_after(&t->link, t->uac->transactions.prev);
	locker_unlock(&t->uac->locker);

	// message
	t->size = sip_message_write(t->req, t->data, sizeof(t->data));
	if (t->size < 0 || t->size >= sizeof(t->data))
	{
		sip_uac_transaction_release(t);
		return NULL;
	}

	// 8.1.2 Sending the Request (p41)

	// 12.2.1.1 Generating the Request (p73)
	// 1. If the route set is empty, the UAC MUST place the remote target URI into 
	//    the Request-URI. The UAC MUST NOT add a Route header field to the request.
	// 2. If the route set is not empty, and the first URI in the route set contains 
	//    the lr parameter, the UAC MUST place the remote target URI into the 
	//    Request-URI and MUST include a Route header field containing the route 
	//    set values in order, including all parameters.
	// 3. If the route set is not empty, and its first URI does not contain the
	//    lr parameter, the UAC MUST place the first URI from the route set into 
	//    the Request-URI, stripping any parameters that are not allowed in a Request-URI.
	//    The UAC MUST add a Route header field containing the remainder of the 
	//	  route set values in order, including all parameters. The UAC MUST then 
	//    place the remote target URI into the Route header field as the last value.
	//	  METHOD sip:proxy1
	//	  Route: <sip:proxy2>,<sip:proxy3;lr>,<sip:proxy4>,<sip:user@remoteua>
	// 4. A UAC SHOULD include a Contact header field in any target refresh
	//    requests within a dialog, and unless there is a need to change it,

	// 18.1.1 Sending Requests (p142)

	// Before a request is sent, the client transport MUST insert a value of
	// the "sent-by" field into the Via header field.

	return onsend(param, t->data, t->size);
}

struct sip_uac_transaction_t* sip_uac_register(struct sip_uac_t* uac, const char* name, int seconds, sip_uac_onreply onregister, void* param)
{
	struct sip_uac_transaction_t* t;
	t = sip_uac_transaction_create(uac);
	t->req = sip_message_create(SIP_METHOD_REGISTER, name, name);
	t->onreply = onregister;
	t->param = param;
	return t;
}

struct sip_uac_transaction_t* sip_uac_options(struct sip_uac_t* uac, const char* name, const char* to, sip_uac_onreply onoptins, void* param)
{
	struct sip_uac_transaction_t* t;
	t = sip_uac_transaction_create(uac);
	t->req = sip_message_create(SIP_METHOD_OPTIONS, name, to);
	t->onreply = onoptins;
	t->param = param;
	return t;
}

struct sip_uac_transaction_t* sip_uac_invite(struct sip_uac_t* uac, const char* name, const char* to, sip_uac_oninvite* oninvite, void* param)
{
	struct sip_uac_transaction_t* t;
	t = sip_uac_transaction_create(uac);
	t->req = sip_message_create(SIP_METHOD_INVITE, name, to);
	t->oninvite = oninvite;
	t->param = param;
	return t;
}

struct sip_uac_transaction_t* sip_uac_cancel(struct sip_uac_t* uac, void* session, sip_uac_onreply oncancel, void* param)
{
	struct sip_dialog_t* dialog;
	struct sip_uac_transaction_t* t;
	dialog = (struct sip_dialog_t*)session;

	t = sip_uac_transaction_create(uac);
	t->req = sip_message_create(SIP_METHOD_CANCEL, name, to);
	t->onreply = oncancel;
	t->param = param;
	return t;
}

struct sip_uac_transaction_t* sip_uac_bye(struct sip_uac_t* uac, void* session, sip_uac_onreply onbye, void* param)
{
	struct sip_dialog_t* dialog;
	struct sip_uac_transaction_t* t;
	dialog = (struct sip_dialog_t*)session;

	t = sip_uac_transaction_create(uac);
	t->req = sip_message_create(SIP_METHOD_BYE, name, to);
	t->onreply = onbye;
	t->param = param;
	return t;
}

struct sip_uac_transaction_t* sip_uac_reinvite(struct sip_uac_t* uac, void* session, sip_uac_oninvite* oninvite, void* param)
{
	struct sip_dialog_t* dialog;
	struct sip_uac_transaction_t* t;
	dialog = (struct sip_dialog_t*)session;

	t = sip_uac_transaction_create(uac);
	t->req = sip_message_create(SIP_METHOD_INVITE, name, to);
	t->oninvite = oninvite;
	t->param = param;
	return t;
}

int sip_uac_get_header_count(struct sip_uac_transaction_t* t)
{
	return sip_params_count(&t->reply->headers);
}

int sip_uac_get_header(struct sip_uac_transaction_t* t, int i, const char** name, const char** value)
{
	struct sip_param_t* param;
	param = sip_params_get(&t->reply->headers, i);
	if (!param) return -1;
	name = &param->name.p;
	value = &param->value.p;
	return 0;
}

const char* sip_uac_get_header_by_name(struct sip_uac_transaction_t* t, const char* name)
{
	return sip_params_find(&t->reply->headers, name, strlen(name));
}

int sip_uac_add_header(struct sip_uac_transaction_t* t, const char* name, const char* value)
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
		r = sip_header_contact(value, end, &t->req->from);
	}
	else if (0 == strcasecmp(SIP_HEADER_TO, name))
	{
		r = sip_header_contact(value, end, &t->req->to);
	}
	else if (0 == strcasecmp(SIP_HEADER_CALLID, name))
	{
		t->req->callid.p = value;
		t->req->callid.n = end - value;
	}
	else if (0 == strcasecmp(SIP_HEADER_CSEQ, name))
	{
		r = sip_header_cseq(value, end, &t->req->cseq);
	}
	else if (0 == strcasecmp(SIP_HEADER_MAX_FORWARDS, name))
	{
		t->req->maxforwards = strtol(value, NULL, 10);
	}
	else if (0 == strcasecmp(SIP_HEADER_VIA, name))
	{
		r = sip_header_via(value, end, &t->req->vias);
	}
	else if (0 == strcasecmp(SIP_HEADER_CONTACT, name))
	{
		r = sip_header_contacts(value, end, &t->req->contacts);
	}
	else if (0 == strcasecmp(SIP_HEADER_ROUTE, name))
	{
		memset(&uri, 0, sizeof(uri)); 
		r = sip_header_uri(value, end, &uri);
		if (0 == r)
			sip_uris_push(&t->req->routers, &uri);
	}
	else if (0 == strcasecmp(SIP_HEADER_RECORD_ROUTE, name))
	{
		memset(&uri, 0, sizeof(uri));
		r = sip_header_uri(value, end, &uri);
		if (0 == r)
			sip_uris_push(&t->req->record_routers, &uri);
	}
	else
	{
		header.name.p = name;
		header.name.n = strlen(name);
		header.value.p = value ? value : "";
		header.value.n = value ? strlen(value) : 0;
		return sip_params_push(&t->req->headers, &header);
	}
}
