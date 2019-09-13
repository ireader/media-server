#include "sip-uac.h"
#include "../sip-internal.h"
#include "sip-uac-transaction.h"
#include "sip-timer.h"
#include "sip-header.h"
#include "sip-dialog.h"
#include "sip-message.h"
#include "sip-transport.h"
#include <stdio.h>

int sip_uac_add_transaction(struct sip_agent_t* sip, struct sip_uac_transaction_t* t)
{
	atomic_increment32(&sip->ref); // ref by transaction
	assert(sip->ref > 0);

	// link to tail
	locker_lock(&sip->locker);
	list_insert_after(&t->link, sip->uac.prev);
	locker_unlock(&sip->locker);
	return sip_uac_transaction_addref(t);
}

int sip_uac_del_transaction(struct sip_agent_t* sip, struct sip_uac_transaction_t* t)
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
		if (cstreq(&t->req->callid, &dialog->callid) && DIALOG_ERALY == dialog->state)
		{
			//assert(0 == sip_contact_compare(&t->req->from, &dialog->local.uri));
			sip_dialog_remove(sip, dialog); // TODO: release in locker
		}
	}

	locker_unlock(&sip->locker);
	sip_agent_destroy(sip);
	return sip_uac_transaction_release(t);
}

void* sip_uac_start_timer(struct sip_agent_t* sip, struct sip_uac_transaction_t* t, int timeout, sip_timer_handle handler)
{
	void* id;

	// wait for timer done
	if (sip_uac_transaction_addref(t) < 2)
	{
		assert(0);
		return NULL;
	}

	id = sip_timer_start(timeout, handler, t);
	if (id == NULL) 
		sip_uac_transaction_release(t);
    assert(id);
	return id;
	//return uac->timer.start(uac->timerptr, timeout, handler, usrptr);
}

void sip_uac_stop_timer(struct sip_agent_t* sip, struct sip_uac_transaction_t* t, void* id)
{
	//if(0 == uac->timer.stop(uac->timerptr, id))
	if (0 == sip_timer_stop(id))
		sip_uac_transaction_release(t);
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

		sip_uac_transaction_addref(t); // add ref
		assert(t->ref >= 2);
		return t;
	}

	return NULL;
}

int sip_uac_input(struct sip_agent_t* sip, struct sip_message_t* reply)
{
	int r;
	struct sip_uac_transaction_t* t;

	// A UAC MUST treat any provisional response different than 100 that it
	// does not recognize as 183 (Session Progress). A UAC MUST be able to
	// process 100 and 183 responses.

	// 8.1.3.3 Vias (p43)
	// If more than one Via header field value is present in a response, the
	// UAC SHOULD discard the message.
	if (1 != sip_vias_count(&reply->vias))
		return 0;

	// 1. fetch transaction
	locker_lock(&sip->locker);
	t = sip_uac_find_transaction(&sip->uac, reply);
	locker_unlock(&sip->locker);
	if (!t)
	{
		// timeout response, discard
		return 0;
	}
	
	// 8.1.3.4 Processing 3xx Responses (p43)
	// Upon receipt of a redirection response (for example, a 301 response
	// status code), clients SHOULD use the URI(s) in the Contact header
	// field to formulate one or more new requests based on the redirected
	// request.
	if (300 <= reply->u.s.code && reply->u.s.code < 400)
	{
		// TODO or create new transaction ???
	}

	// 8.1.3.5 Processing 4xx Responses (p45)
	switch (reply->u.s.code)
	{
	case 401: break; // Unauthorized
	case 407: break; // Proxy Authentication Required
	case 413: break; // Request Entity Too Large
	case 415: break; // Unsupported Media Type
	case 416: break; // Unsupported URI Scheme
	case 420: break; // Bad Extension
	default:  break;
	}

	locker_lock(&t->locker);

	if (sip_message_isinvite(reply))
		r = sip_uac_transaction_invite_input(t, reply);
	else
		r = sip_uac_transaction_noninvite_input(t, reply);

	locker_unlock(&t->locker);
	sip_uac_transaction_release(t);
	return r;
}

int sip_uac_send(struct sip_uac_transaction_t* t, const void* sdp, int bytes, struct sip_transport_t* transport, void* param)
{
	int r;
	char via[1024];
	char contact[1024];
	
	if (t->transportptr)
		return -1; // EEXIST
	memcpy(&t->transport, transport, sizeof(struct sip_transport_t));
	t->transportptr = param;

	r = sip_uac_transaction_via(t, via, sizeof(via), contact, sizeof(contact));
	if (0 != r)
		return r;

	// Via
	if (0 == sip_vias_count(&t->req->vias))
	{
		// The Via header maddr, ttl, and sent-by components will be set when
		// the request is processed by the transport layer (Section 18).

		// Via: SIP/2.0/UDP erlang.bell-telephone.com:5060;branch=z9hG4bK87asdks7
		// Via: SIP/2.0/UDP first.example.com:4000;ttl=16;maddr=224.2.0.1;branch=z9hG4bKa7c6a8dlze.1
		r = sip_message_add_header(t->req, "Via", via);
	}
	
	// Contact: <sip:bob@192.0.2.4>
	if (0 == sip_contacts_count(&t->req->contacts) && 
		(sip_message_isinvite(t->req) || sip_message_isregister(t->req) || sip_message_isrefer(t->req)))
	{
		// 12.1.2 UAC Behavior (p71)
		// When a UAC sends a request that can establish a dialog (such as an
		// INVITE) it MUST provide a SIP or SIPS URI with global scope (i.e.,
		// the same SIP URI can be used in messages outside this dialog) in the
		// Contact header field of the request.

		// The Contact header field MUST be present and contain exactly one SIP or 
		// SIPS URI in any request that can result in the establishment of a dialog.
		// For the methods defined in this specification, that includes only the INVITE request.

		// While the Via header field tells other elements where to send the
		// response, the Contact header field tells other elements where to send
		// future requests.

		// usually composed of a username at a fully qualified domain name(FQDN)
		// If the Request-URI or top Route header field value contains a SIPS
		// URI, the Contact header field MUST contain a SIPS URI as well.
		r = sip_message_add_header(t->req, "Contact", contact);
	}

	// get transport reliable from via protocol
	t->reliable = 1;
	if (sip_vias_count(&t->req->vias) > 0 && !sip_transport_isreliable(&(sip_vias_get(&t->req->vias, 0)->transport)))
	{
		t->reliable = 0;
	}

	// message
	t->req->payload = sdp;
	t->req->size = bytes;
	t->size = sip_message_write(t->req, t->data, sizeof(t->data));
	if (t->size < 0 || t->size >= sizeof(t->data))
		return -1;

	return sip_uac_transaction_send(t);
}

int sip_uac_transaction_via(struct sip_uac_transaction_t* t, char *via, int nvia, char *contact, int ncontact)
{
	int r;
	char dns[128];
	char local[128];
	char remote[256]; // destination/router
	char protocol[16];
	struct cstring_t user;
	const struct sip_uri_t* uri;

	uri = sip_message_get_next_hop(t->req);
	if (!uri || cstrcpy(&uri->host, remote, sizeof(remote)) >= sizeof(remote) - 1)
		return -1;

	// rfc3263 4-Client Usage (p5)
	// once a SIP server has successfully been contacted (success is defined below), 
	// all retransmissions of the SIP request and the ACK for non-2xx SIP responses 
	// to INVITE MUST be sent to the same host.
	// Furthermore, a CANCEL for a particular SIP request MUST be sent to the same 
	// SIP server that the SIP request was delivered to.
	protocol[0] = local[0] = dns[0] = 0;
	r = t->transport.via(t->transportptr, remote, protocol, local, dns);
	if (0 != r)
		return r;

	if (NULL == strchr(dns, '.'))
		snprintf(dns, sizeof(dns), "%s", local); // don't have valid dns

	// Via
	// Via: SIP/2.0/UDP erlang.bell-telephone.com:5060;branch=z9hG4bK87asdks7
	// Via: SIP/2.0/UDP first.example.com:4000;ttl=16;maddr=224.2.0.1;branch=z9hG4bKa7c6a8dlze.1
	r = snprintf(via, nvia, "SIP/2.0/%s %s;branch=%s%p%d%s", protocol, dns, SIP_BRANCH_PREFIX, t, rand(), sip_transport_isreliable2(protocol)?"":";rport");
	if (r < 0 || r >= nvia)
		return -1; // ENOMEM

	// Contact
	// usually composed of a username at a fully qualified domain name(FQDN)
	// If the Request-URI or top Route header field value contains a SIPS
	// URI, the Contact header field MUST contain a SIPS URI as well.
	if (0 == sip_uri_username(&t->req->from.uri, &user))
	{
		assert(user.n > 0);
		r = snprintf(contact, ncontact, "<%.*s:%.*s@%s>", uri->scheme.n > 0 ? (int)uri->scheme.n : 3, uri->scheme.n > 0 ? uri->scheme.p : "sip", (int)user.n, user.p, local);
		if (r < 0 || r >= ncontact)
			return -1; // ENOMEM
	}

	return 0;
}

int sip_uac_add_header(struct sip_uac_transaction_t* t, const char* name, const char* value)
{
	return sip_message_add_header(t->req, name, value);
}

int sip_uac_add_header_int(struct sip_uac_transaction_t* t, const char* name, int value)
{
	return sip_message_add_header_int(t->req, name, value);
}

int sip_uac_transaction_ondestroy(struct sip_uac_transaction_t* t, sip_transaction_ondestroy ondestroy, void* param)
{
    t->ondestroy = ondestroy;
    t->ondestroyparam = param;
    return 0;
}
