#ifndef _sip_uac_h_
#define _sip_uac_h_

#include "sip-timer.h"
#include "sip-header.h"
#include "sip-dialog.h"
#include "sip-message.h"
#include "sip-transport.h"
#include "sys/atomic.h"
#include "sys/locker.h"
#include "list.h"

struct sip_uac_transaction_t;

struct sip_uac_t
{
	int32_t ref;
	locker_t locker;
	struct sip_contact_t user; // sip from

	struct sip_transport_t* transport;
	void* transportptr;

	struct sip_timer_t timer;
	void* timerptr;

	struct list_head transactions; // transaction layer handler
	struct list_head dialogs; // early or confirmed dialogs
};

/// @param[in] name such as: "Alice <sip:alice@atlanta.com>"
struct sip_uac_t* sip_uac_create(const char* name);
int sip_uac_destroy(struct sip_uac_t* uac);
int sip_uac_input(struct sip_uac_t* uac, struct sip_message_t* reply);

typedef int (*sip_uac_oninvite)(void* param, struct sip_uac_transaction_t* t, struct sip_dialog_t* dialog, int code);
struct sip_uac_transaction_t* sip_uac_invite(struct sip_uac_t* uac, const char* to, const char* sdp, sip_uac_oninvite* oninvite, void* param);

typedef int(*sip_uac_oncancel)(void* param, int code);
int sip_uac_cancel(struct sip_uac_t* uac, struct sip_uac_transaction_t* t);

typedef int(*sip_uac_onbye)(void* param, int code);
int sip_uac_bye(struct sip_uac_t* uac, struct sip_dialog_t* dialog);

int sip_uac_ack(struct sip_uac_t* uac, struct sip_dialog_t* dialog, const char* sdp);

typedef int(*sip_uac_onregister)(void* param, int code);
int sip_uac_register(struct sip_uac_t* uac, int expiration);

typedef int(*sip_uac_onoptions)(void* param, int code);
int sip_uac_options(struct sip_uac_t* uac);

#endif /* !_sip_uac_h_ */
