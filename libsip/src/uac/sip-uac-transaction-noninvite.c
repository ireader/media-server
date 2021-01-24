/*
|Request from TU
								   |send request
			   Timer E             V
			   send request  +-----------+
				   +---------|           |-------------------+
				   |         |  Trying   |  Timer F          |
				   +-------->|           |  or Transport Err.|
							 +-----------+  inform TU        |
				200-699         |  |                         |
				resp. to TU     |  |1xx                      |
				+---------------+  |resp. to TU              |
				|                  |                         |
				|   Timer E        V       Timer F           |
				|   send req +-----------+ or Transport Err. |
				|  +---------|           | inform TU         |
				|  |         |Proceeding |------------------>|
				|  +-------->|           |-----+             |
				|            +-----------+     |1xx          |
				|              |      ^        |resp to TU   |
				| 200-699      |      +--------+             |
				| resp. to TU  |                             |
				|              |                             |
				|              V                             |
				|            +-----------+                   |
				|            |           |                   |
				|            | Completed |                   |
				|            |           |                   |
				|            +-----------+                   |
				|              ^   |                         |
				|              |   | Timer K                 |
				+--------------+   | -                       |
								   |                         |
								   V                         |
			 NOTE:           +-----------+                   |
							 |           |                   |
		 transitions         | Terminated|<------------------+
		 labeled with        |           |
		 the event           +-----------+
		 over the action
		 to take

				 Figure 6: non-INVITE client transaction
*/

#include "sip-uac-transaction.h"
#include "sip-uac.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int sip_uac_onbye(struct sip_uac_transaction_t* t, const struct sip_message_t* reply);
int sip_uac_oncancel(struct sip_uac_transaction_t* t, const struct sip_message_t* reply);

static int sip_uac_transaction_noninvite_proceeding(struct sip_uac_transaction_t* t, const struct sip_message_t* reply)
{
	assert(SIP_UAC_TRANSACTION_TRYING == t->status || SIP_UAC_TRANSACTION_PROCEEDING == t->status);
	if(sip_message_issubscribe(t->req))
		return sip_uac_subscribe_onreply(t, reply);
	else if (sip_message_isnotify(t->req))
		return sip_uac_notify_onreply(t, reply);
    else
		return t->onreply(t->param, reply, t, reply->u.s.code);
}

static int sip_uac_transaction_noninvite_completed(struct sip_uac_transaction_t* t, const struct sip_message_t* reply)
{
	int r;
	assert(SIP_UAC_TRANSACTION_TRYING == t->status || SIP_UAC_TRANSACTION_PROCEEDING == t->status || SIP_UAC_TRANSACTION_COMPLETED == t->status);

	// stop retry timer A
	sip_uac_stop_timer(t->agent, t, &t->timera);

	if(sip_message_issubscribe(t->req))
		r = sip_uac_subscribe_onreply(t, reply);
	else if (sip_message_isnotify(t->req))
		r = sip_uac_notify_onreply(t, reply);
	else
		r = t->onreply(t->param, reply, t, reply->u.s.code);
	
	// post-handle
    if(sip_message_isbye(t->req))
        sip_uac_onbye(t, reply);
    else if(sip_message_iscancel(t->req))
        sip_uac_oncancel(t, reply);
//	if (t->onhandle)
//		t->onhandle(t, reply);

	// wait for in-flight response
	sip_uac_transaction_timewait(t, t->reliable ? 1 : TIMER_K);
	return r;
}

// Figure 6: non-INVITE client transaction (p133)
static int sip_uac_transaction_noinivte_change_state(struct sip_uac_transaction_t* t, const struct sip_message_t* reply)
{
	switch (t->status)
	{
	case SIP_UAC_TRANSACTION_TRYING:
	case SIP_UAC_TRANSACTION_PROCEEDING:
		assert(100 <= reply->u.s.code && reply->u.s.code < 700);
		if (100 <= reply->u.s.code && reply->u.s.code < 200)
			t->status = SIP_UAC_TRANSACTION_PROCEEDING;
		else if (200 <= reply->u.s.code && reply->u.s.code < 700)
			t->status = SIP_UAC_TRANSACTION_COMPLETED;
		break;

	case SIP_UAC_TRANSACTION_COMPLETED:
		// duplicated packet, discard
		assert(100 <= reply->u.s.code && reply->u.s.code < 700);
		break;

	case SIP_UAC_TRANSACTION_TERMINATED:
		// nothing to do
		break;

	default:
		assert(0);
	}

	return t->status;
}

int sip_uac_transaction_noninvite_input(struct sip_uac_transaction_t* t, const struct sip_message_t* reply)
{
	int r, status, oldstatus;

	oldstatus = t->status;
	status = sip_uac_transaction_noinivte_change_state(t, reply);

	r = 0;
	switch (status)
	{
	case SIP_UAC_TRANSACTION_TRYING:
	case SIP_UAC_TRANSACTION_PROCEEDING:
		r = sip_uac_transaction_noninvite_proceeding(t, reply);
		break;

	case SIP_UAC_TRANSACTION_COMPLETED:
		if(oldstatus != status)
			r = sip_uac_transaction_noninvite_completed(t, reply);
		break;

	case SIP_UAC_TRANSACTION_TERMINATED:
	default:
		r = -1;
		assert(0);
		break;
	}

	return r;
}
