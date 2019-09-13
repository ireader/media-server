#include "sip-subscribe.h"
#include "sip-internal.h"
#include "sip-message.h"
#include "sip-subscribe.h"
#include "sys/atomic.h"
#include "sys/locker.h"
#include <stdlib.h>

// 12.1.2 UAC Behavior
// A UAC MUST be prepared to receive a response without a tag in the To
// field, in which case the tag is considered to have a value of null.
// This is to maintain backwards compatibility with RFC 2543, which
// did not mandate To tags.
static const struct cstring_t sc_null = { "", 0 };

struct sip_subscribe_t* sip_subscribe_create(const struct cstring_t* event)
{
	struct sip_subscribe_t* s;
	s = (struct sip_subscribe_t*)calloc(1, sizeof(struct sip_subscribe_t));
	if (s)
	{
		s->ref = 1;
		LIST_INIT_HEAD(&s->link);
		s->state = SUBSCRIBE_INIT;
		cstrcpy(event, s->event, sizeof(s->event) - 1);
	}
	return s;
}

int sip_subscribe_release(struct sip_subscribe_t* subscribe)
{
	if (!subscribe)
		return -1;

	assert(subscribe->ref > 0);
	if (0 != atomic_decrement32(&subscribe->ref))
		return 0;

	if (subscribe->dialog)
		sip_dialog_release(subscribe->dialog);
	free(subscribe);
	return 0;
}

int sip_subscribe_addref(struct sip_subscribe_t* subscribe)
{
	assert(subscribe->ref > 0);
	return atomic_increment32(&subscribe->ref);
}

/// @return 1-match, 0-don't match
static int sip_subscribe_match(const struct sip_subscribe_t* subscribe, const struct cstring_t* callid, const struct cstring_t* local, const struct cstring_t* remote, const struct cstring_t *event)
{
	assert(subscribe && local);
	if (!remote) remote = &sc_null;

	return cstreq(callid, &subscribe->dialog->callid) && cstreq(local, &subscribe->dialog->local.uri.tag) && cstreq(remote, &subscribe->dialog->remote.uri.tag) && 0==cstrcmp(event, subscribe->event) ? 1 : 0;
}

static struct sip_subscribe_t* sip_subscribe_find(struct sip_agent_t* sip, const struct cstring_t* callid, const struct cstring_t* local, const struct cstring_t* remote, const struct cstring_t *event)
{
	struct list_head *pos, *next;
	struct sip_subscribe_t* subscribe;

	list_for_each_safe(pos, next, &sip->subscribes)
	{
		subscribe = list_entry(pos, struct sip_subscribe_t, link);
		if (sip_subscribe_match(subscribe, callid, local, remote, event))
			return subscribe;
	}

	return NULL;
}

int sip_subscribe_add(struct sip_agent_t* sip, struct sip_subscribe_t* subscribe)
{
	struct cstring_t event;
	event.p = subscribe->event;
	event.n = strlen(subscribe->event);

	// TODO:
	// the dialog of subscribe don't link to sip->dialogs, so so so...

	assert(subscribe->dialog);
	locker_lock(&sip->locker);
	if (NULL != sip_subscribe_find(sip, &subscribe->dialog->callid, &subscribe->dialog->local.uri.tag, &subscribe->dialog->remote.uri.tag, &event))
	{
		locker_unlock(&sip->locker);
		return -1; // exist
	}

	// link to tail
	assert(1 == subscribe->ref);
	list_insert_after(&subscribe->link, sip->subscribes.prev);
	locker_unlock(&sip->locker);
	sip_subscribe_addref(subscribe);
	return 0;
}

int sip_subscribe_remove(struct sip_agent_t* sip, struct sip_subscribe_t* subscribe)
{
	// unlink dialog
	locker_lock(&sip->locker);
	//assert(1 == subscribe->ref);
	if (subscribe->newdiaolog)
	{
		subscribe->newdiaolog = 0;
		sip_dialog_remove(sip, subscribe->dialog);
	}
	list_remove(&subscribe->link);
	locker_unlock(&sip->locker);
	sip_subscribe_release(subscribe);
	return 0;
}

struct sip_subscribe_t* sip_subscribe_fetch(struct sip_agent_t* sip, const struct cstring_t* callid, const struct cstring_t* local, const struct cstring_t* remote, const struct cstring_t* event)
{
	struct sip_subscribe_t* subscribe;
	locker_lock(&sip->locker);
	subscribe = sip_subscribe_find(sip, callid, local, remote, event);
	if (subscribe)
		sip_subscribe_addref(subscribe);
	locker_unlock(&sip->locker);
	return subscribe;
}

struct sip_dialog_t* sip_dialog_internal_fetch(struct sip_agent_t* sip, const struct sip_message_t* msg, int uac, int* added);
// internal use only !!!!!!!!!
struct sip_subscribe_t* sip_subscribe_internal_fetch(struct sip_agent_t* sip, const struct sip_message_t* msg, const struct cstring_t* event, int uac, int* added)
{
	struct sip_subscribe_t* subscribe;

	*added = 0;
	locker_lock(&sip->locker);
	subscribe = sip_subscribe_find(sip, &msg->callid, &msg->from.tag, &msg->to.tag, event);
	if (NULL == subscribe)
	{
		subscribe = sip_subscribe_create(event);
		if (!subscribe)
		{
			locker_unlock(&sip->locker);
			return NULL; // exist
		}

		subscribe->dialog = sip_dialog_internal_fetch(sip, msg, uac, &subscribe->newdiaolog);
		if (!subscribe->dialog)
		{
			locker_unlock(&sip->locker);
			sip_subscribe_release(subscribe);
			return NULL; // exist
		}

		// link to tail (add ref later)
		list_insert_after(&subscribe->link, sip->subscribes.prev);
		*added = 1;
	}

	assert(subscribe->dialog);
	locker_unlock(&sip->locker);
	sip_subscribe_addref(subscribe); // for sip link dialog / fetch
	return subscribe;
}
