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

	struct sip_uas_handler_t* handler;
	void* param;
};

struct sip_uas_handler_t
{
	/// @return user-defined session-id, null if error
	void* (*oninvite)(void* param, struct sip_uas_transaction_t* t, const void* data, int bytes);
	/// @return 0-ok, other-error
	int (*onack)(void* param, const void* session, const void* data, int bytes);
	
	/// on terminating a session(dialog)
	int (*onbye)(void* param, struct sip_uas_transaction_t* t, const void* session);
	
	/// cancel a transaction(should be an invite transaction)
	int (*oncancel)(void* param, struct sip_uas_transaction_t* t, const void* session);
	
	int (*onregister)(void* param, struct sip_uas_transaction_t* t, const char* user, const char* location, int expire);

	int (*send)(void* param, const void* data, int bytes);
};

/// @param[in] name such as: "Alice <sip:alice@atlanta.com>"
struct sip_uas_t* sip_uas_create(const char* name, struct sip_uas_handler_t* handler, void* param);
int sip_uas_destroy(struct sip_uas_t* uas);

int sip_uas_input(struct sip_uas_t* uas, const struct sip_message_t* msg);

// valid only on callback
int sip_uas_get_header_count(struct sip_uas_transaction_t* t);
int sip_uas_get_header(struct sip_uas_transaction_t* t, int i, struct cstring_t* const name, struct cstring_t* const value);
const struct cstring_t* sip_uas_get_header_by_name(struct sip_uas_transaction_t* t, const char* name);

int sip_uas_add_header(struct sip_uas_transaction_t* t, const char* name, const char* value);
int sip_uas_add_header_int(struct sip_uas_transaction_t* t, const char* name, int value);

int sip_uas_reply(struct sip_uas_transaction_t* t, int code, const void* data, int bytes);
int sip_uas_discard(struct sip_uas_transaction_t* t);

#endif /* !_sip_uas_h_ */
