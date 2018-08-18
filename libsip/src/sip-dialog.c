#include "sip-dialog.h"
#include "sip-message.h"

// 12.1.2 UAC Behavior
// A UAC MUST be prepared to receive a response without a tag in the To
// field, in which case the tag is considered to have a value of null.
// This is to maintain backwards compatibility with RFC 2543, which
// did not mandate To tags.
static const struct cstring_t sc_null = { "", 0 };

struct sip_dialog_t* sip_dialog_create(const struct sip_message_t* msg)
{
	int i;
	const struct cstring_t* to;
	struct sip_uri_t uri;
	struct sip_dialog_t* dialog;

	to = &msg->to.tag;
	if (!cstrvalid(to)) to = &sc_null;
	assert(cstrvalid(&msg->from.tag));

	dialog = (struct sip_dialog_t*)calloc(1, sizeof(*dialog));
	if (!dialog) return NULL;

	LIST_INIT_HEAD(&dialog->link);
	dialog->state = DIALOG_ERALY;
	cstring_clone(&dialog->callid, &msg->callid);
	sip_uri_clone(&dialog->local.uri, &msg->from.uri);
	sip_uri_clone(&dialog->remote.uri, &msg->to.uri);
	cstring_clone(&dialog->local.tag, &msg->from.tag);
	cstring_clone(&dialog->remote.tag, to);
	dialog->local.id = msg->cseq.id;
	dialog->remote.id = -1;

	assert(1 == sip_contacts_count(&msg->contacts));
	if (sip_contacts_count(&msg->contacts) > 0)
	{
		sip_uri_clone(&dialog->target, &sip_contacts_get(&msg->contacts, 1)->uri);
	}

	for (i = 0; i < sip_uris_count(&msg->routers); i++)
	{
		sip_uri_clone(&uri, sip_uris_get(&msg->routers, i));
		sip_uris_push(&dialog->routers, &uri);
	}

	dialog->secure = cstrprefix(&dialog->target.host, "sips");

	return dialog;
}

int sip_dialog_destroy(struct sip_dialog_t* dialog)
{
	int i;
	if (!dialog)
		return -1;

	cstring_free(&dialog->callid);
	cstring_free(&dialog->local.tag);
	cstring_free(&dialog->remote.tag);
	sip_uri_free(&dialog->local.uri);
	sip_uri_free(&dialog->remote.uri);
	sip_uri_free(&dialog->target);

	for (i = 0; i < sip_uris_count(&dialog->routers); i++)
		sip_uri_free(sip_uris_get(&dialog->routers, i));

	free(dialog);
	return 0;
}

int sip_dialog_match(const struct sip_dialog_t* dialog, const struct cstring_t* callid, const struct cstring_t* from, const struct cstring_t* to)
{
	assert(dialog && from);
	if (!to) to = &sc_null;

	return cstreq(callid, &dialog->callid) && cstreq(from, &dialog->local.tag) && cstreq(to, &dialog->remote.tag) ? 1 : 0;
}

struct sip_dialog_t* sip_dialog_find(struct list_head* dialogs, struct sip_message_t* req)
{
	struct list_head *pos, *next;
	struct sip_dialog_t* dialog;
	
	list_for_each_safe(pos, next, dialogs)
	{
		dialog = list_entry(pos, struct sip_dialog_t, link);
		if(sip_dialog_match(dialog, &req->callid, &req->from.tag, &req->to.tag))
			return dialog;
	}

	return NULL;
}
