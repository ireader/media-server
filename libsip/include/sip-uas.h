#ifndef _sip_uas_h_
#define _sip_uas_h_

#include "cstring.h"

struct sip_uas_t;
struct sip_dialog_t;
struct sip_message_t;
struct sip_uas_transaction_t;

struct sip_uas_handler_t
{
	/// @return user-defined session-id, null if error
	void* (*oninvite)(void* param, struct sip_uas_transaction_t* t, const void* data, int bytes);
	/// @return 0-ok, other-error
	/// @param[in] code 0-ok, other-sip status code
	int (*onack)(void* param, struct sip_uas_transaction_t* t, const void* session, struct sip_dialog_t* dialog, int code, const void* data, int bytes);
	
	/// on terminating a session(dialog)
	int (*onbye)(void* param, struct sip_uas_transaction_t* t, const void* session);
	
	/// cancel a transaction(should be an invite transaction)
	int (*oncancel)(void* param, struct sip_uas_transaction_t* t, const void* session);
	
	/// @param[in] expires in seconds
	int (*onregister)(void* param, struct sip_uas_transaction_t* t, const char* user, const char* location, int expires);

	/// @return <0-error, 0-udp, 1-tcp, other-reserved
	int (*send)(void* param, const struct cstring_t* url, const struct cstring_t* received, const void* data, int bytes);
};

/// @param[in] name such as: "Alice <sip:alice@atlanta.com>"
struct sip_uas_t* sip_uas_create(const char* name, struct sip_uas_handler_t* handler, void* param);
int sip_uas_destroy(struct sip_uas_t* uas);

int sip_uas_input(struct sip_uas_t* uas, const struct sip_message_t* msg);

int sip_uas_add_header(struct sip_uas_transaction_t* t, const char* name, const char* value);
int sip_uas_add_header_int(struct sip_uas_transaction_t* t, const char* name, int value);

int sip_uas_reply(struct sip_uas_transaction_t* t, int code, const void* data, int bytes);
int sip_uas_discard(struct sip_uas_transaction_t* t);

#endif /* !_sip_uas_h_ */
