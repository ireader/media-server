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
        dialog->state = DIALOG_ERALY;
        dialog->ptr = (char*)(dialog + 1);
		atomic_increment32(&s_gc.dialog);
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
	atomic_decrement32(&s_gc.dialog);
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
	{
		sip_uri_free(&dialog->local.target);
		dialog->ptr = sip_uri_clone(dialog->ptr, end, &dialog->local.target, &contact->uri);
	}
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

int sip_dialog_id(struct cstring_t* id, const struct sip_dialog_t* dialog, char* ptr, int len)
{
	int r;
	r = dialog ? snprintf(ptr, len, "%.*s@%.*s@%.*s", (int)dialog->callid.n, dialog->callid.p, (int)dialog->local.uri.tag.n, dialog->local.uri.tag.p, (int)dialog->remote.uri.tag.n, dialog->remote.uri.tag.p) : 0;
	id->p = ptr;
	id->n = r > 0 && r < sizeof(id) ? r : 0;
	return r;
}

int sip_dialog_id_with_message(struct cstring_t *id, const struct sip_message_t* msg, char* ptr, int len)
{
	int r;
	if (msg->mode == SIP_MESSAGE_REQUEST)
		r = snprintf(ptr, len, "%.*s@%.*s@%.*s", (int)msg->callid.n, msg->callid.p, (int)msg->to.tag.n, msg->to.tag.p, (int)msg->from.tag.n, msg->from.tag.p);
	else
		r = snprintf(ptr, len, "%.*s@%.*s@%.*s", (int)msg->callid.n, msg->callid.p, (int)msg->from.tag.n, msg->from.tag.p, (int)msg->to.tag.n, msg->to.tag.p);
	
	id->p = ptr;
	id->n = r > 0 && r < sizeof(id) ? r : 0;
	return r;
}
