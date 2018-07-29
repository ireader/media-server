#ifndef _sip_uas_h_
#define _sip_uas_h_

#include "sip-timer.h"
#include "sip-header.h"
#include "sip-dialog.h"
#include "sip-message.h"
#include "sip-transport.h"
#include "sys/atomic.h"
#include "sys/locker.h"
#include "list.h"

struct sip_uas_transaction_t;

struct sip_uas_t
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

struct sip_uas_handler_t
{
	int (*ondialog)(void* param, struct sip_uas_transaction_t* t, struct sip_dialog_t* dialog, struct sip_message_t* msg);

	int (*onmsg)(void* param, struct sip_uas_transaction_t* t, struct sip_message_t* msg);

	int (*send)(void* param, const void* data, int bytes);
};

/// @param[in] name such as: "Alice <sip:alice@atlanta.com>"
struct sip_uas_t* sip_uas_create(const char* name);
int sip_uas_destroy(struct sip_uas_t* uas);

int sip_uas_input(struct sip_uas_t* uas, struct sip_message_t* request);

int sip_uas_reply(struct sip_uas_transaction_t* t, int code, const void* data, int bytes);

int sip_uas_add_header(struct sip_uas_transaction_t* t, const char* name, const char* value);

#endif /* !_sip_uas_h_ */
