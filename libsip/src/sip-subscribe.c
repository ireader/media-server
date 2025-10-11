/*
* 4.1.  Subscriber Behavior
* https://www.rfc-editor.org/rfc/rfc6665#section-4.1.2
					
						  +-------------+
						  |    init     |<-----------------------+
						  +-------------+                        |
								 |                           Retry-after
						   Send SUBSCRIBE                    expires
								 |                               |
								 V          Timer N Fires;       |
						  +-------------+   SUBSCRIBE failure    |
			 +------------| notify_wait |-- response; --------+  |
			 |            +-------------+   or NOTIFY,        |  |
			 |                   |          state=terminated  |  |
			 |                   |                            |  |
   ++========|===================|============================|==|====++
   ||        |                   |                            V  |    ||
   ||  Receive NOTIFY,    Receive NOTIFY,             +-------------+ ||
   ||  state=active       state=pending               | terminated  | ||
   ||        |                   |                    +-------------+ ||
   ||        |                   |          Re-subscription     A  A  ||
   ||        |                   V          times out;          |  |  ||
   ||        |            +-------------+   Receive NOTIFY,     |  |  ||
   ||        |            |   pending   |-- state=terminated; --+  |  ||
   ||        |            +-------------+   or 481 response        |  ||
   ||        |                   |          to SUBSCRIBE           |  ||
   ||        |            Receive NOTIFY,   refresh                |  ||
   ||        |            state=active                             |  ||
   ||        |                   |          Re-subscription        |  ||
   ||        |                   V          times out;             |  ||
   ||        |            +-------------+   Receive NOTIFY,        |  ||
   ||        +----------->|   active    |-- state=terminated; -----+  ||
   ||                     +-------------+   or 481 response           ||
   ||                                       to SUBSCRIBE              ||
   || Subscription                          refresh                   ||
   ++=================================================================++


* 4.2.  Notifier Behavior
* https://www.rfc-editor.org/rfc/rfc6665#section-4.2.2

						 +-------------+
						 |    init     |
						 +-------------+
								|
						  Receive SUBSCRIBE,
						  Send NOTIFY
								|
								V          NOTIFY failure,
						 +-------------+   subscription expires,
			+------------|  resp_wait  |-- or terminated ----+
			|            +-------------+   per local policy  |
			|                   |                            |
			|                   |                            |
			|                   |                            V
	  Policy grants       Policy needed              +-------------+
	  permission                |                    | terminated  |
			|                   |                    +-------------+
			|                   |                               A A
			|                   V          NOTIFY failure,      | |
			|            +-------------+   subscription expires,| |
			|            |   pending   |-- or terminated -------+ |
			|            +-------------+   per local policy       |
			|                   |                                 |
			|            Policy changed to                        |
			|            grant permission                         |
			|                   |                                 |
			|                   V          NOTIFY failure,        |
			|            +-------------+   subscription expires,  |
			+----------->|   active    |-- or terminated ---------+
						 +-------------+   per local policy

*/

#include "sip-subscribe.h"
#include "sip-internal.h"
#include "sip-message.h"
#include "sip-subscribe.h"
#include "sys/atomic.h"
#include "sys/locker.h"
#include <stdlib.h>

#define N 512

// 12.1.2 UAC Behavior
// A UAC MUST be prepared to receive a response without a tag in the To
// field, in which case the tag is considered to have a value of null.
// This is to maintain backwards compatibility with RFC 2543, which
// did not mandate To tags.
static const struct cstring_t sc_null = { "", 0 };

struct sip_subscribe_t* sip_subscribe_create(const struct sip_event_t* event)
{
	char* end;
	struct sip_subscribe_t* s;
	s = (struct sip_subscribe_t*)calloc(1, sizeof(*s) + N);
	if (s)
	{
		s->ref = 1;
		s->state = SUBSCRIBE_INIT;
		s->ptr = (char*)(s + 1);

		end = s->ptr + N;
		s->ptr = cstring_clone(s->ptr, end, &s->event.event, event->event.p, event->event.n);
		s->ptr = cstring_clone(s->ptr, end, &s->event.id, event->id.p, event->id.n);
		atomic_increment32(&s_gc.subscribe);
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

	//sip_event_free(&subscribe->event); // event->params don't init
	free(subscribe);
	atomic_decrement32(&s_gc.subscribe);
	return 0;
}

int sip_subscribe_addref(struct sip_subscribe_t* subscribe)
{
	assert(subscribe->ref > 0);
	return atomic_increment32(&subscribe->ref);
}

/// @return 1-match, 0-don't match
static int sip_subscribe_match(const struct sip_subscribe_t* subscribe, const struct cstring_t* callid, const struct cstring_t* local, const struct cstring_t* remote, const struct sip_event_t* event)
{
	assert(subscribe && local);
	if (!remote) remote = &sc_null;

	return cstreq(callid, &subscribe->dialog->callid) && cstreq(local, &subscribe->dialog->local.uri.tag) && cstreq(remote, &subscribe->dialog->remote.uri.tag) && sip_event_equal(event, &subscribe->event) ? 1 : 0;
}

struct sip_subscribe_t* sip_subscribe_internal_create(struct sip_agent_t* sip, const struct sip_message_t* msg, const struct sip_event_t* event, int uac)
{
	struct sip_subscribe_t* subscribe;
	subscribe = sip_subscribe_create(event);
	if (!subscribe)
	{
		locker_unlock(&sip->locker);
		return NULL; // exist
	}

	subscribe->dialog = sip_dialog_create();
	if (!subscribe->dialog || 0 != (uac ? sip_dialog_init_uac(subscribe->dialog, msg) : sip_dialog_init_uas(subscribe->dialog, msg)))
	{
		sip_subscribe_release(subscribe);
		return NULL;
	}
	subscribe->dialog->state = DIALOG_CONFIRMED; // confirm dialog
	return subscribe;
}

int sip_subscribe_id(struct cstring_t* id, const struct sip_subscribe_t* subscribe, char* ptr, int len)
{
	int r;
	r = subscribe ? snprintf(ptr, len, "%.*s@%.*s@%.*s@%.*s@%.*s", (int)subscribe->dialog->callid.n, subscribe->dialog->callid.p, (int)subscribe->dialog->local.uri.tag.n, subscribe->dialog->local.uri.tag.p, (int)subscribe->dialog->remote.uri.tag.n, subscribe->dialog->remote.uri.tag.p, (int)subscribe->event.event.n, subscribe->event.event.p, (int)subscribe->event.id.n, subscribe->event.id.p) : 0;
	id->p = ptr;
	id->n = r > 0 && r < sizeof(id) ? r : 0;
	return r;
}

// @param[in] uas 1-local is uas
int sip_subscribe_id_with_message(struct cstring_t* id, const struct sip_message_t* msg, char* ptr, int len, int uas)
{
	int r;
	assert(msg->mode == SIP_MESSAGE_REQUEST);
	if (uas)
		r = snprintf(ptr, len, "%.*s@%.*s@%.*s@%.*s@%.*s", (int)msg->callid.n, msg->callid.p, (int)msg->to.tag.n, msg->to.tag.p, (int)msg->from.tag.n, msg->from.tag.p, (int)msg->event.event.n, msg->event.event.p, (int)msg->event.id.n, msg->event.id.p);
	else
		r = snprintf(ptr, len, "%.*s@%.*s@%.*s@%.*s@%.*s", (int)msg->callid.n, msg->callid.p, (int)msg->from.tag.n, msg->from.tag.p, (int)msg->to.tag.n, msg->to.tag.p, (int)msg->event.event.n, msg->event.event.p, (int)msg->event.id.n, msg->event.id.p);

	id->p = ptr;
	id->n = r > 0 && r < sizeof(id) ? r : 0;
	return r;
}
