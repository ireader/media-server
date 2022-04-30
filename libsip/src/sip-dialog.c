#include "sip-dialog.h"
#include "sip-message.h"
#include "sip-internal.h"
#include "sys/atomic.h"
#include "sys/locker.h"
#include <stdlib.h>

#define N 2048

// 12.1.2 UAC Behavior
// A UAC MUST be prepared to receive a response without a tag in the To
// field, in which case the tag is considered to have a value of null.
// This is to maintain backwards compatibility with RFC 2543, which
// did not mandate To tags.
static const struct cstring_t sc_null = { "", 0 };

struct sip_dialog_t* sip_dialog_create(void)
{
    struct sip_dialog_t* dialog;
    
    dialog = (struct sip_dialog_t*)calloc(1, sizeof(*dialog)+ N);
    if (dialog)
    {
        dialog->ref = 1;
        LIST_INIT_HEAD(&dialog->link);
        dialog->state = DIALOG_ERALY;
        dialog->ptr = (char*)(dialog + 1);
    }
    return dialog;
}

int sip_dialog_init_uac(struct sip_dialog_t* dialog, const struct sip_message_t* msg)
{
	int i;
	char *end;
	struct sip_uri_t uri;
	const struct sip_contact_t* contact;

    assert(SIP_MESSAGE_REPLY == msg->mode);
	assert(cstrvalid(&msg->from.tag) && cstrvalid(&msg->to.tag));
	end = dialog->ptr + N;

	dialog->ptr = cstring_clone(dialog->ptr, end, &dialog->callid, msg->callid.p, msg->callid.n);
	dialog->ptr = sip_contact_clone(dialog->ptr, end, &dialog->local.uri, &msg->from);
	dialog->ptr = sip_contact_clone(dialog->ptr, end, &dialog->remote.uri, &msg->to);
	dialog->local.id = msg->cseq.id;
	dialog->local.rseq = rand();
	dialog->remote.id = rand();
	dialog->remote.rseq = rand();

	//assert(1 == sip_contacts_count(&msg->contacts));
	contact = sip_contacts_get(&msg->contacts, 0);
	if (contact && cstrvalid(&contact->uri.host))
		dialog->ptr = sip_uri_clone(dialog->ptr, end, &dialog->remote.target, &contact->uri);

	// 12.1.2 UAC Behavior (p71)
	// The route set MUST be set to the list of URIs in the Record-Route
	// header field from the response, taken in reverse order and preserving
	// all URI parameters.
	sip_uris_init(&dialog->routers);
	for (i = sip_uris_count(&msg->record_routers); i > 0; i--)
	{
		dialog->ptr = sip_uri_clone(dialog->ptr, end, &uri, sip_uris_get(&msg->record_routers, i-1));
		sip_uris_push(&dialog->routers, &uri);
	}

	dialog->secure = cstrprefix(&dialog->remote.target.host, "sips");
	return 0;
}

int sip_dialog_init_uas(struct sip_dialog_t* dialog, const struct sip_message_t* msg)
{
    int i;
    char *end;
    struct sip_uri_t uri;
    const struct sip_contact_t* contact;
    
    assert(SIP_MESSAGE_REQUEST == msg->mode);
    assert(cstrvalid(&msg->from.tag));
    end = dialog->ptr + N;

	dialog->ptr = cstring_clone(dialog->ptr, end, &dialog->callid, msg->callid.p, msg->callid.n);
    dialog->ptr = sip_contact_clone(dialog->ptr, end, &dialog->local.uri, &msg->to);
    dialog->ptr = sip_contact_clone(dialog->ptr, end, &dialog->remote.uri, &msg->from);
    dialog->local.id = rand();
	dialog->local.rseq = rand();
    dialog->remote.id = msg->cseq.id;
	dialog->remote.rseq = 0==msg->rseq ? rand() : msg->rseq;
    
    //assert(1 == sip_contacts_count(&msg->contacts));
    contact = sip_contacts_get(&msg->contacts, 0);
    if (contact && cstrvalid(&contact->uri.host))
        dialog->ptr = sip_uri_clone(dialog->ptr, end, &dialog->remote.target, &contact->uri);
    
	// 12.1.1 UAS behavior (p70)
	// The route set MUST be set to the list of URIs in the Record-Route
	// header field from the request, taken in order and preserving all URI
	// parameters. If no Record-Route header field is present in the
	// request, the route set MUST be set to the empty set.
    sip_uris_init(&dialog->routers);
    for (i = 0; i < sip_uris_count(&msg->record_routers); i++)
    {
        dialog->ptr = sip_uri_clone(dialog->ptr, end, &uri, sip_uris_get(&msg->record_routers, i));
        sip_uris_push(&dialog->routers, &uri);
    }
    
    dialog->secure = cstrprefix(&dialog->remote.target.host, "sips");
    return 0;
}

int sip_dialog_release(struct sip_dialog_t* dialog)
{
	if (!dialog)
		return -1;

	assert(dialog->ref > 0);
	if (0 != atomic_decrement32(&dialog->ref))
		return 0;

	sip_uri_free(&dialog->local.target);
	sip_contact_free(&dialog->local.uri);
	sip_uri_free(&dialog->remote.target);
	sip_contact_free(&dialog->remote.uri);
	sip_uris_free(&dialog->routers);
	free(dialog);
	return 0;
}

int sip_dialog_addref(struct sip_dialog_t* dialog)
{
	int r;
	r = atomic_increment32(&dialog->ref);
	assert(r > 1);
	return r;
}

int sip_dialog_setlocaltag(struct sip_dialog_t* dialog, const struct cstring_t* tag)
{
	const char* end;
	end = (char*)(dialog + 1) + N;
	dialog->ptr = cstring_clone(dialog->ptr, end, &dialog->local.uri.tag, tag->p, tag->n);
	sip_params_add_or_update(&dialog->local.uri.params, "tag", 3, &dialog->local.uri.tag);
	return dialog->ptr < end ? 0 : -1;
}

int sip_dialog_set_local_target(struct sip_dialog_t* dialog, const struct sip_message_t* msg)
{
	const char* end;
	struct sip_contact_t* contact;
	end = (char*)(dialog + 1) + N;

	contact = sip_contacts_get(&msg->contacts, 0);
	if (contact && cstrvalid(&contact->uri.host) && !sip_uri_equal(&dialog->local.target, &contact->uri))
		dialog->ptr = sip_uri_clone(dialog->ptr, end, &dialog->local.target, &contact->uri);
	return dialog->ptr < end ? 0 : -1;
}

int sip_dialog_target_refresh(struct sip_dialog_t* dialog, const struct sip_message_t* msg)
{
    const char* end;
    struct sip_contact_t* contact;
    end = (char*)(dialog + 1) + N;
    
    contact = sip_contacts_get(&msg->contacts, 0);
	if (contact && cstrvalid(&contact->uri.host) && !sip_uri_equal(&dialog->remote.target, &contact->uri))
	{
		sip_uri_free(&dialog->remote.target);
		dialog->ptr = sip_uri_clone(dialog->ptr, end, &dialog->remote.target, &contact->uri);
	}
    return dialog->ptr < end ? 0 : -1;
}

/// @return 1-match, 0-don't match
static int sip_dialog_match(const struct sip_dialog_t* dialog, const struct cstring_t* callid, const struct cstring_t* local, const struct cstring_t* remote)
{
	assert(dialog && local);
	if (!remote) remote = &sc_null;

	return cstreq(callid, &dialog->callid) && cstreq(local, &dialog->local.uri.tag) && cstreq(remote, &dialog->remote.uri.tag) ? 1 : 0;
}

static struct sip_dialog_t* sip_dialog_find(struct sip_agent_t* sip, const struct cstring_t* callid, const struct cstring_t* local, const struct cstring_t* remote)
{
	struct list_head *pos, *next;
	struct sip_dialog_t* dialog;

	list_for_each_safe(pos, next, &sip->dialogs)
	{
		dialog = list_entry(pos, struct sip_dialog_t, link);
		if (sip_dialog_match(dialog, callid, local, remote))
			return dialog;
	}
	
	return NULL;
}

struct sip_dialog_t* sip_dialog_fetch(struct sip_agent_t* sip, const struct cstring_t* callid, const struct cstring_t* local, const struct cstring_t* remote)
{
	struct sip_dialog_t* dialog;
	locker_lock(&sip->locker);
	dialog = sip_dialog_find(sip, callid, local, remote);
	if(dialog)
		sip_dialog_addref(dialog);
	locker_unlock(&sip->locker);
	return dialog;
}

int sip_dialog_add(struct sip_agent_t* sip, struct sip_dialog_t* dialog)
{
	locker_lock(&sip->locker);
	if (NULL != sip_dialog_find(sip, &dialog->callid, &dialog->local.uri.tag, &dialog->remote.uri.tag))
	{
		assert(0);
		locker_unlock(&sip->locker);
		return -1; // exist
	}

	// link to tail
	assert(1 == dialog->ref);
	list_insert_after(&dialog->link, sip->dialogs.prev);
	locker_unlock(&sip->locker);
	sip_dialog_addref(dialog);
	return 0;
}

int sip_dialog_remove(struct sip_agent_t* sip, struct sip_dialog_t* dialog)
{
	// unlink dialog
	locker_lock(&sip->locker);
	//assert(1 == dialog->ref);
	if (dialog->link.next == NULL)
	{
		// fix remove twice
		locker_unlock(&sip->locker);
		return -1;
	}

	list_remove(&dialog->link);
	locker_unlock(&sip->locker);
	sip_dialog_release(dialog);
	return 0;
}

int sip_dialog_remove2(struct sip_agent_t* sip, const struct cstring_t* callid, const struct cstring_t* local, const struct cstring_t* remote)
{
	struct sip_dialog_t* dialog;
	locker_lock(&sip->locker);
	dialog = sip_dialog_find(sip, callid, local, remote);
	if (dialog && dialog->link.next)
		list_remove(&dialog->link);
	locker_unlock(&sip->locker);

	if (dialog)
	{
		sip_dialog_release(dialog);
		return 0;
	}
	return -1; // not found
}

int sip_dialog_remove_early(struct sip_agent_t* sip, const struct cstring_t* callid)
{
	struct sip_dialog_t* early;
	struct sip_dialog_t* dialog;
	struct list_head *pos, *next;

	early = NULL;
	locker_lock(&sip->locker);
	list_for_each_safe(pos, next, &sip->dialogs)
	{
		dialog = list_entry(pos, struct sip_dialog_t, link);
		if (cstreq(callid, &dialog->callid) && DIALOG_ERALY == dialog->state)
		{
			//assert(0 == sip_contact_compare(&t->req->from, &dialog->local.uri));
			list_remove(&dialog->link);
			early = dialog;
			break;
		}
	}

	locker_unlock(&sip->locker);
	if (early)
	{
		sip_dialog_release(early);
		return 0;
	}
	return -1; // not found
}

// MUST ADD LOCK !!!!! internal use only !!!!!!!!!
struct sip_dialog_t* sip_dialog_internal_fetch(struct sip_agent_t* sip, const struct sip_message_t* msg, int uac, int* added)
{
	struct sip_dialog_t* dialog;

	*added = 0;
    dialog = sip_dialog_find(sip, &msg->callid, uac ? &msg->from.tag : &msg->to.tag, uac ? &msg->to.tag : &msg->from.tag);
	if (!dialog)
	{
		dialog = sip_dialog_create();
		if (!dialog || 0 != (uac ? sip_dialog_init_uac(dialog, msg) : sip_dialog_init_uas(dialog, msg)))
		{
			sip_dialog_release(dialog);
			return NULL;
		}

		// link to sip dialogs(add ref later)
		list_insert_after(&dialog->link, sip->dialogs.prev);
		assert(dialog->ref == 1);
		*added = 1;
	}

	sip_dialog_addref(dialog); // for sip link dialog / fetch
	return dialog;
}
