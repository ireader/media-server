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

	//struct sip_timer_t timer;
	//void* timerptr;

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
		//sip_uac_transaction_release(t);
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
	//return sip_uac_transaction_addref(t);
	return 0;
}

int sip_uac_del_transaction(struct sip_uac_t* uac, struct sip_uac_transaction_t* t)
{
	struct sip_dialog_t* dialog;
	struct list_head *pos, *next;

	assert(uac->ref > 0);
	locker_lock(&uac->locker);

	// unlink transaction
	list_remove(&t->link);

	// 12.3 Termination of a Dialog (p77)
	// Independent of the method, if a request outside of a dialog generates
	// a non-2xx final response, any early dialogs created through
	// provisional responses to that request are terminated.
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
	//return sip_uac_transaction_release(t);
	return 0;
}

void* sip_uac_start_timer(struct sip_uac_t* uac, int timeout, sip_timer_handle handler, void* usrptr)
{
	//return uac->timer.start(uac->timerptr, timeout, handler, usrptr);
	return sip_timer_start(timeout, handler, usrptr);
}

void sip_uac_stop_timer(struct sip_uac_t* uac, void* id)
{
	//uac->timer.stop(uac->timerptr, id);
	sip_timer_stop(id);
}

// RFC3261 17.1.3 Matching Responses to Client Transactions (p132)
static struct sip_uac_transaction_t* sip_uac_find_transaction(struct list_head* transactions, struct sip_message_t* reply)
{
	const struct cstring_t *p, *p2;
	struct list_head *pos, *next;
	struct sip_uac_transaction_t* t;

	p = sip_vias_top_branch(&reply->vias);
	if (!p) return NULL;
	assert(cstrprefix(p, SIP_BRANCH_PREFIX));

	list_for_each_safe(pos, next, transactions)
	{
		t = list_entry(pos, struct sip_uac_transaction_t, link);

		// 1. via branch parameter
		p2 = sip_vias_top_branch(&t->req->vias);
		if (!p2 || 0 == cstreq(p, p2))
			continue;
		assert(cstrprefix(p2, SIP_BRANCH_PREFIX));
		
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
	char ptr[1024];
	char dns[128];
	char local[128];
	char remote[256]; // destination/router
	char protocol[16];
	struct cstring_t user;
	const struct sip_uri_t* uri;

	if (t->transportptr)
		return -1; // EEXIST

	// 1. get transport local info
	if (0 == sip_vias_count(&t->req->vias) || 0 == sip_contacts_count(&t->req->contacts))
	{
		uri = sip_message_get_next_hop(t->req);
		if (!uri || cstrcpy(&uri->host, remote, sizeof(remote)) >= sizeof(remote) - 1)
			return -1;

		protocol[0] = local[0] = dns[0] = 0;
		r = transport->via(param, remote, protocol, local, dns);
		if (0 != r)
			return r;
	}

	// 2. Via
	if (0 == sip_vias_count(&t->req->vias))
	{
		// Via: SIP/2.0/UDP erlang.bell-telephone.com:5060;branch=z9hG4bK87asdks7
		// Via: SIP/2.0/UDP first.example.com:4000;ttl=16;maddr=224.2.0.1;branch=z9hG4bKa7c6a8dlze.1
		r = snprintf(ptr, sizeof(ptr), "SIP/2.0/%s %s;branch=%s%p", protocol, dns, SIP_BRANCH_PREFIX, t);
		if (r < 0 || r >= sizeof(ptr))
			return -1; // ENOMEM
		r = sip_message_add_header(t->req, "Via", ptr);
	}
	
	// 3. Contact: <sip:bob@192.0.2.4>
	if (0 == sip_contacts_count(&t->req->contacts) && 0 == sip_uri_username(&t->req->from.uri, &user) && user.n < sizeof(remote)
		&& (sip_message_isinvite(t->req) || sip_message_isregister(t->req)))
	{
		// The Contact header field MUST be present and contain exactly one SIP or 
		// SIPS URI in any request that can result in the establishment of a dialog.
		// For the methods defined in this specification, that includes only the INVITE request.

		// While the Via header field tells other elements where to send the
		// response, the Contact header field tells other elements where to send
		// future requests.

		// usually composed of a username at a fully qualified domain name(FQDN)
		cstrcpy(&user, remote, sizeof(remote));
		snprintf(ptr, sizeof(ptr), "<sip:%s@%s>", remote, dns);
		r = sip_message_add_header(t->req, "Contact", ptr);
	}

	// 4. get transport reliable from via protocol
	t->reliable = 0;
	if (sip_vias_count(&t->req->vias) > 0)
	{
		t->reliable = cstrcmp(&(sip_vias_get(&t->req->vias, 0)->transport), "UDP");
	}

	// message
	t->req->payload = sdp;
	t->req->size = bytes;
	t->size = sip_message_write(t->req, t->data, sizeof(t->data));
	if (t->size < 0 || t->size >= sizeof(t->data))
	{
		sip_uac_transaction_destroy(t);
		return -1;
	}

	memcpy(&t->transport, transport, sizeof(struct sip_transport_t));
	t->transportptr = param;
	atomic_increment32(&t->uac->ref); // ref by transaction
	return sip_uac_transaction_send(t);
}

int sip_uac_add_header(struct sip_uac_transaction_t* t, const char* name, const char* value)
{
	return sip_message_add_header(t->req, name, value);
}

int sip_uac_add_header_int(struct sip_uac_transaction_t* t, const char* name, int value)
{
	return sip_message_add_header_int(t->req, name, value);
}
