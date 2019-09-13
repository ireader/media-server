#include "sip-uas.h"
#include "../sip-internal.h"
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

void* sip_uas_start_timer(struct sip_agent_t* sip, struct sip_uas_transaction_t* t, int timeout, sip_timer_handle handler)
{
	void* id;

	// wait for timer done
	if (sip_uas_transaction_addref(t) < 2)
		return NULL;

	id = sip_timer_start(timeout, handler, t);
	if (id == NULL)
		sip_uas_transaction_release(t);
    assert(id);
	return id;
	//return uas->timer.start(uas->timerptr, timeout, handler, usrptr);
}

void sip_uas_stop_timer(struct sip_agent_t* sip, struct sip_uas_transaction_t* t, void* id)
{
	//uas->timer.stop(uas->timerptr, id);
	if (0 == sip_timer_stop(id))
		sip_uas_transaction_release(t);
}

int sip_uas_add_transaction(struct sip_agent_t* sip, struct sip_uas_transaction_t* t)
{
	t->param = sip->param;
	t->handler = &sip->handler;
	
	assert(sip->ref > 0);
	atomic_increment32(&sip->ref); // ref by transaction

	// link to tail
	locker_lock(&sip->locker);
	list_insert_after(&t->link, sip->uas.prev);
	locker_unlock(&sip->locker);
	return sip_uas_transaction_addref(t);
}

int sip_uas_del_transaction(struct sip_agent_t* sip, struct sip_uas_transaction_t* t)
{
	struct sip_dialog_t* dialog;
	struct list_head *pos, *next;

	assert(sip->ref > 0);
	locker_lock(&sip->locker);

	// unlink transaction
	list_remove(&t->link);

	// 12.3 Termination of a Dialog (p77)
	// Independent of the method, if a request outside of a dialog generates
	// a non-2xx final response, any early dialogs created through
	// provisional responses to that request are terminated.
	list_for_each_safe(pos, next, &sip->dialogs)
	{
		dialog = list_entry(pos, struct sip_dialog_t, link);
		if (cstreq(&t->reply->callid, &dialog->callid) && DIALOG_ERALY == dialog->state)
		{
			//assert(0 == sip_contact_compare(&t->req->from, &dialog->local.uri));
			sip_dialog_remove(sip, dialog); // TODO: release in locker
		}
	}

	locker_unlock(&sip->locker);
	sip_agent_destroy(sip); // unref by transaction
	return sip_uas_transaction_release(t);
}

static struct sip_uas_transaction_t* sip_uas_find_acktransaction(struct sip_agent_t* sip, const struct sip_message_t* req)
{
	struct list_head *pos, *next;
	struct sip_uas_transaction_t* t;

	list_for_each_safe(pos, next, &sip->uas)
	{
		t = list_entry(pos, struct sip_uas_transaction_t, link);
		if (cstreq(&t->reply->callid, &req->callid) && cstreq(&t->reply->from.tag, &req->from.tag) && cstreq(&t->reply->to.tag, &req->to.tag))
		{
			sip_uas_transaction_addref(t);
			return t;
		}
	}

	return NULL;
}

// RFC3261 17.2.3 Matching Requests to Server Transactions (p138)
struct sip_uas_transaction_t* sip_uas_find_transaction(struct sip_agent_t* sip, const struct sip_message_t* req, int matchmethod)
{
	struct list_head *pos, *next;
	struct sip_uas_transaction_t* t;
	const struct sip_via_t *via, *via2;

	via = sip_vias_get(&req->vias, 0);
	if (!via) return NULL; // invalid sip message
	assert(cstrprefix(&via->branch, SIP_BRANCH_PREFIX));

	list_for_each_safe(pos, next, &sip->uas)
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
		if(!cstreq(&via->host, &via2->host) || !cstreq(&req->callid, &t->reply->callid))
			continue;

		// 3. cseq method parameter
		// the method of the request matches the one that created the
		// transaction, except for ACK, where the method of the request
		// that created the transaction is INVITE
		assert(cstreq(&req->cseq.method, &t->reply->cseq.method) || 0 == cstrcasecmp(&req->cseq.method, SIP_METHOD_CANCEL) || 0 == cstrcasecmp(&req->cseq.method, SIP_METHOD_ACK));
		if (matchmethod && cstreq(&req->cseq.method, &t->reply->cseq.method))
		{
			assert(req->cseq.id == t->reply->cseq.id);
			sip_uas_transaction_addref(t);
			return t;
		}

		// ACK/CANCEL find origin request transaction
		if (!matchmethod && !cstreq(&req->cseq.method, &t->reply->cseq.method) && 0 != cstrcasecmp(&t->reply->cseq.method, SIP_METHOD_CANCEL) && 0 != cstrcasecmp(&t->reply->cseq.method, SIP_METHOD_ACK))
		{
			assert(req->cseq.id == t->reply->cseq.id);
			sip_uas_transaction_addref(t);
			return t;
		}
	}

	// The ACK for a 2xx response to an INVITE request is a separate transaction
	return sip_message_isack(req) ? sip_uas_find_acktransaction(sip, req) : NULL;
}

//static int sip_uas_check_uri(struct sip_uas_t* uas, struct sip_uas_transaction_t* t, const struct sip_message_t* msg)
//{
//	// 8.2.2.1 To and Request-URI
//	// if the To header field does not address a known or current user of this UAS
//	if (0 != cstrcasecmp(&msg->to.uri.host, ""))
//		return sip_uas_reply(t, 403/*Forbidden*/, NULL, 0);
//	// If the Request-URI uses a scheme not supported by the UAS
//	if (0 != cstrcasecmp(&msg->u.c.uri.scheme, "sip") && 0 != cstrcasecmp(&msg->u.c.uri.scheme, "sips"))
//		return sip_uas_reply(t, 416/*Unsupported URI Scheme*/, NULL, 0);
//	// If the Request-URI does not identify an address that the UAS is willing to accept requests for
//	if (0 != cstrcasecmp(&msg->u.c.uri.host, ""))
//		return sip_uas_reply(t, 404/*Not Found*/, NULL, 0);
//	return 0;
//}
//// 8.2.2.3 Require (p47)
//static int sip_uas_check_require(struct sip_uas_t* uas, struct sip_uas_transaction_t* t, const struct sip_message_t* msg)
//{
//	const struct cstring_t* header;
//
//	header = sip_message_get_header_by_name(msg, "Require");
//	sip_uas_add_header(t, "Unsupported", );
//	return sip_uas_reply(t, 420/*Bad Extension*/, NULL, 0);
//}
//// 8.2.3 Content Processing (p47)
//static int sip_uas_check_media_type(struct sip_uas_t* uas, struct sip_uas_transaction_t* t, const struct sip_message_t* msg)
//{
//	const struct cstring_t* content_type;
//	const struct cstring_t* content_language;
//	const struct cstring_t* content_encoding;
//
//	content_type = sip_message_get_header_by_name(msg, "Content-Type");
//	content_language = sip_message_get_header_by_name(msg, "Content-Language");
//	content_encoding = sip_message_get_header_by_name(msg, "Content-Encoding");
//	sip_uas_add_header(t, "Accept", );
//	sip_uas_add_header(t, "Accept-Encoding", );
//	sip_uas_add_header(t, "Accept-Language", );
//	return sip_uas_reply(t, 415/*Unsupported Media Type*/, NULL, 0);
//}

static int sip_uas_check_request(struct sip_agent_t* sip, struct sip_uas_transaction_t* t, const struct sip_message_t* msg)
{
	//int r;
	
	// 8.1.1.6 Max-Forwards
	// If the Max-Forwards value reaches 0 before the request reaches its 
	// destination, it will be rejected with a 483(Too Many Hops) error response.
	if (msg->maxforwards <= 0)
		return sip_uas_reply(t, 483/*Too Many Hops*/, NULL, 0);

	//r = sip_uas_check_uri(uas, t, msg);
	//if (0 == r)
	//	r = sip_uas_check_require(uas, t, msg);
	//if (0 == r)
	//	r = sip_uas_check_media_type(uas, t, msg);

	return 0;
}

int sip_uas_input(struct sip_agent_t* sip, const struct sip_message_t* msg)
{
	int r;
	struct sip_dialog_t *dialog;
	struct sip_uas_transaction_t* t;

	// 1. find transaction
	locker_lock(&sip->locker);
	t = sip_uas_find_transaction(sip, msg, 1);
	if (!t)
	{
		if (sip_message_isack(msg))
		{
			locker_unlock(&sip->locker);
			return 0; // invalid ack, discard, TODO: add log here
		}

		t = sip_uas_transaction_create(sip, msg);
		if (!t)
		{
			locker_unlock(&sip->locker);
			return -1;
		}
	}
	locker_unlock(&sip->locker);

	r = sip_uas_check_request(sip, t, msg);
	if (0 != r)
	{
		sip_uas_transaction_release(t);
		return r;
	}

	// 2. find dialog
	dialog = sip_dialog_fetch(sip, &msg->callid, &msg->to.tag, &msg->from.tag);
    
    locker_lock(&t->locker);

	// 4. handle
	if (sip_message_isinvite(msg) || sip_message_isack(msg))
		r = sip_uas_transaction_invite_input(t, dialog, msg);
	else
		r = sip_uas_transaction_noninvite_input(t, dialog, msg);

	// TODO:
	// 1. A stateless UAS MUST NOT send provisional (1xx) responses.
	// 2. A stateless UAS MUST NOT retransmit responses.
	// 3. A stateless UAS MUST ignore ACK requests.
	// 4. A stateless UAS MUST ignore CANCEL requests.

	locker_unlock(&t->locker);
	sip_uas_transaction_release(t);
	if (dialog)
		sip_dialog_release(dialog);
	return r;
}

int sip_uas_reply(struct sip_uas_transaction_t* t, int code, const void* data, int bytes)
{
    int r;
    locker_lock(&t->locker);
    
    // Contact: <sip:bob@192.0.2.4>
    if (0 == sip_contacts_count(&t->reply->contacts) &&
        (sip_message_isinvite(t->reply) || sip_message_isregister(t->reply)))
    {
		// 12.1.1 UAS behavior (p70)
		// The UAS MUST add a Contact header field to the response. The Contact 
		// header field contains an address where the UAS would like to be 
		// contacted for subsequent requests in the dialog (which includes the 
		// ACK for a 2xx response in the case of an INVITE). Generally, the host 
		// portion of this URI is the IP address or FQDN of the host. 
		// The URI provided in the Contact header field MUST be a SIP or SIPS URI.

        sip_message_set_reply_default_contact(t->reply);
    }
    
	// get transport reliable from via protocol
	t->reliable = 1;
	if (sip_vias_count(&t->reply->vias) > 0
		&& (0 == cstrcmp(&(sip_vias_get(&t->reply->vias, 0)->transport), "UDP")
			|| 0 == cstrcmp(&(sip_vias_get(&t->reply->vias, 0)->transport), "DTLS")))
	{
		t->reliable = 0;
	}

	if (sip_message_isinvite(t->reply))
	{
		r = sip_uas_transaction_invite_reply(t, code, data, bytes);
	}
	else
	{
		r = sip_uas_transaction_noninvite_reply(t, code, data, bytes);
	}
    locker_unlock(&t->locker);
    return r;
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

int sip_uas_transaction_ondestroy(struct sip_uas_transaction_t* t, sip_transaction_ondestroy ondestroy, void* param)
{
    t->ondestroy = ondestroy;
    t->ondestroyparam = param;
    return 0;
}
