#ifndef _sip_subscribe_h_
#define _sip_subscribe_h_

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
	char event[128];

	// internal use only
	struct list_head link;
	void* evtsession; // user-defined event session
	int32_t ref;
	int newdiaolog; // flag for remote dialog from sip dialogs link
};

struct sip_subscribe_t* sip_subscribe_create(const struct cstring_t* event);
int sip_subscribe_release(struct sip_subscribe_t* subscribe);
int sip_subscribe_addref(struct sip_subscribe_t* subscribe);

// subscribe management
int sip_subscribe_add(struct sip_agent_t* sip, struct sip_subscribe_t* subscribe);
int sip_subscribe_remove(struct sip_agent_t* sip, struct sip_subscribe_t* subscribe);

/// call sip_subscribe_release
struct sip_subscribe_t* sip_subscribe_fetch(struct sip_agent_t* sip, const struct cstring_t* callid, const struct cstring_t* local, const struct cstring_t* remote, const struct cstring_t* event);

/// call sip_subscribe_release
struct sip_subscribe_t* sip_subscribe_internal_fetch(struct sip_agent_t* sip, const struct sip_message_t* msg, const struct cstring_t* event, int uac, int* added);

#endif /* !_sip_subscribe_h_ */
