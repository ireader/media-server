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

struct sip_agent_t
{
	int32_t ref;
	locker_t locker;

	//struct sip_timer_t timer;
	//void* timerptr;

	struct list_head dialogs;
	
	struct list_head uac; // uac transactions
	struct list_head uas; // uas transactions
	struct sip_uas_handler_t handler;
	void* param;
};

int sip_uac_input(struct sip_agent_t* sip, struct sip_message_t* reply);
int sip_uas_input(struct sip_agent_t* sip, const struct sip_message_t* request);

#endif /* !_sip_internal_h_ */
