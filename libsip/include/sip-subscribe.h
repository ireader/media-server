#ifndef _sip_subscribe_h_
#define _sip_subscribe_h_

#if defined(__cplusplus)
extern "C" {
#endif

#include "sip-dialog.h"

#define SIP_SUBSCRIPTION_STATE_ACTIVE		"active"
#define SIP_SUBSCRIPTION_STATE_PENDING		"pending"
#define SIP_SUBSCRIPTION_STATE_TERMINATED	"terminated"

// rfc6665 4.1.3. Receiving and Processing State Information (p15)
#define SIP_SUBSCRIPTION_REASON_DEACTIVATED	"deactivated"
#define SIP_SUBSCRIPTION_REASON_PROBATION	"probation"
#define SIP_SUBSCRIPTION_REASON_REJECTED	"rejected"
#define SIP_SUBSCRIPTION_REASON_TIMEOUT		"timeout"
#define SIP_SUBSCRIPTION_REASON_GIVEUP		"giveup"
#define SIP_SUBSCRIPTION_REASON_NORESOURCE	"noresource"
#define SIP_SUBSCRIPTION_REASON_INVARIANT	"invariant"

enum {
	SUBSCRIBE_INIT = 0,
	SUBSCRIBE_ACTIVE,
	SUBSCRIBE_TERMINATED,
};

struct sip_subscribe_t
{
	struct sip_dialog_t* dialog;
	int state; // SUBSCRIBE_INIT

	uint64_t expires;
	struct sip_event_t event;
	
	// internal use only
	char* ptr;
	int32_t ref;
};

struct sip_subscribe_t* sip_subscribe_create(const struct sip_event_t* event);
int sip_subscribe_release(struct sip_subscribe_t* subscribe);
int sip_subscribe_addref(struct sip_subscribe_t* subscribe);

struct sip_subscribe_t* sip_subscribe_internal_create(struct sip_agent_t* sip, const struct sip_message_t* msg, const struct sip_event_t* event, int uac);

int sip_subscribe_id(struct cstring_t* id, const struct sip_subscribe_t* subscribe, char* ptr, int len);
int sip_subscribe_id_with_message(struct cstring_t* id, const struct sip_message_t* msg, char* ptr, int len, int uas);

#if defined(__cplusplus)
}
#endif
#endif /* !_sip_subscribe_h_ */
