#ifndef _sip_internal_h_
#define _sip_internal_h_

#include "sip-agent.h"
#include "sip-message.h"
#include "sys/atomic.h"
#include "sys/locker.h"
#include "list.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct sip_gc_t
{
	int32_t uac; // uac transaction
	int32_t uas; // uas transaction
	int32_t dialog; // dialog
	int32_t message; // dialog
	int32_t subscribe;
};

struct sip_agent_t
{
	int32_t ref;
	locker_t locker;

	//struct sip_timer_t timer;
	//void* timerptr;

	struct list_head dialogs;
	struct list_head subscribes;
	
	struct list_head uac; // uac transactions
	struct list_head uas; // uas transactions
	struct sip_uas_handler_t handler;
};

int sip_uac_input(struct sip_agent_t* sip, struct sip_message_t* reply);
int sip_uas_input(struct sip_agent_t* sip, const struct sip_message_t* request, void* param);

static inline int sip_transport_isreliable(const struct cstring_t* c)
{
	return (0 == cstrcasecmp(c, "TCP") || 0 == cstrcasecmp(c, "TLS") || 0 == cstrcasecmp(c, "SCTP")) ? 1 : 0;
}
static inline int sip_transport_isreliable2(const char* protocol)
{
	struct cstring_t c;
	c.p = protocol;
	c.n = strlen(protocol);
	return sip_transport_isreliable(&c);
}

extern struct sip_gc_t s_gc;

#endif /* !_sip_internal_h_ */
