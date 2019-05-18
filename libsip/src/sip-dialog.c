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

struct sip_dialog_t* sip_dialog_create(const struct sip_message_t* msg)
{
	int i;
	char *end;
	struct sip_uri_t uri;
	struct sip_dialog_t* dialog;
	const struct sip_contact_t* contact;
	//const struct cstring_t* to;

	//to = &msg->to.tag;
	//if (!cstrvalid(to)) to = &sc_null;
	assert(cstrvalid(&msg->from.tag));

	dialog = (struct sip_dialog_t*)calloc(1, sizeof(*dialog)+ N);
	if (!dialog) return NULL;

	LIST_INIT_HEAD(&dialog->link);
	dialog->state = DIALOG_ERALY;
	dialog->ptr = (char*)(dialog + 1);
	end = dialog->ptr + N;

	cstrcpy(&msg->callid, dialog->callid, sizeof(dialog->callid));
	//cstrcpy(&msg->from.tag, dialog->local.tag, sizeof(dialog->local.tag));
	//cstrcpy(to, dialog->remote.tag, sizeof(dialog->remote.tag));
	//ptr = sip_uri_clone(ptr, end, &dialog->local.uri, &msg->from.uri);
	//ptr = sip_uri_clone(ptr, end, &dialog->remote.uri, &msg->to.uri);
	dialog->ptr = sip_contact_clone(dialog->ptr, end, &dialog->local.uri, &msg->from);
	dialog->ptr = sip_contact_clone(dialog->ptr, end, &dialog->remote.uri, &msg->to);
	dialog->local.id = msg->cseq.id;
	dialog->remote.id = rand();

	assert(1 == sip_contacts_count(&msg->contacts));
	contact = sip_contacts_get(&msg->contacts, 0);
	if (contact && cstrvalid(&contact->uri.host))
	{
		dialog->ptr = sip_uri_clone(dialog->ptr, end, &dialog->target, &contact->uri);
	}

	sip_uris_init(&dialog->routers);
	for (i = 0; i < sip_uris_count(&msg->routers); i++)
	{
		dialog->ptr = sip_uri_clone(dialog->ptr, end, &uri, sip_uris_get(&msg->routers, i));
		sip_uris_push(&dialog->routers, &uri);
	}

	dialog->secure = cstrprefix(&dialog->target.host, "sips");

	return dialog;
}

static int sip_dialog_release(struct sip_dialog_t* dialog)
{
	if (!dialog)
		return -1;

	if (0 != atomic_decrement32(&dialog->ref))
		return 0;

	sip_uris_free(&dialog->routers);
	free(dialog);
	return 0;
}

static int sip_dialog_addref(struct sip_dialog_t* dialog)
{
	return atomic_increment32(&dialog->ref);
}

int sip_dialog_setremotetag(struct sip_dialog_t* dialog, const struct cstring_t* tag)
{
	const char* end;
	end = (char*)(dialog + 1) + N;
	dialog->ptr = cstring_clone(dialog->ptr, end, &dialog->remote.uri.tag, tag->p, tag->n);
	return 0;
}

/// @return 1-match, 0-don't match
static int sip_dialog_match(const struct sip_dialog_t* dialog, const struct cstring_t* callid, const struct cstring_t* local, const struct cstring_t* remote)
{
	assert(dialog && local);
	if (!remote) remote = &sc_null;

	return 0 == cstrcmp(callid, dialog->callid) && cstreq(local, &dialog->local.uri.tag) && cstreq(remote, &dialog->remote.uri.tag) ? 1 : 0;
}

struct sip_dialog_t* sip_dialog_find(struct sip_agent_t* sip, const struct cstring_t* callid, const struct cstring_t* local, const struct cstring_t* remote)
{
	struct list_head *pos, *next;
	struct sip_dialog_t* dialog;

	locker_lock(&sip->locker);
	list_for_each_safe(pos, next, &sip->dialogs)
	{
		dialog = list_entry(pos, struct sip_dialog_t, link);
		if (sip_dialog_match(dialog, callid, local, remote))
		{
			locker_unlock(&sip->locker);
			return dialog;
		}
	}
	locker_unlock(&sip->locker);
	return NULL;
}

int sip_dialog_add(struct sip_agent_t* sip, struct sip_dialog_t* dialog)
{
	// link to tail
	locker_lock(&sip->locker);
	assert(0 == dialog->ref);
	list_insert_after(&dialog->link, sip->dialogs.prev);
	locker_unlock(&sip->locker);
	return sip_dialog_addref(dialog);
}

int sip_dialog_remove(struct sip_agent_t* sip, struct sip_dialog_t* dialog)
{
	// unlink dialog
	locker_lock(&sip->locker);
	assert(1 == dialog->ref);
	list_remove(&dialog->link);
	locker_unlock(&sip->locker);
	return sip_dialog_release(dialog);
}
