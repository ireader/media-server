#ifndef _sip_agent_h_
#define _sip_agent_h_

#include "cstring.h"

#if defined(__cplusplus)
extern "C" {
#endif

struct sip_agent_t;
struct sip_dialog_t;
struct sip_message_t;
struct sip_subscribe_t;
struct sip_uas_transaction_t;

/// sip UAC/UAS transaction destroy callback
/// @param[in] param user-defined parameter
typedef void (*sip_transaction_ondestroy)(void* param);
    
struct sip_uas_handler_t
{
	/// @param[in] protocol UDP/TCP/TLS/SCTP
	/// @param[in] received replace url host address if has valid value
	/// @param[in] rport valid only rport > 0, if rport <= 0, then use url host port; if host port is empty, then use default sip port
	/// @return 0-ok, <0-error, other-reserved
	int (*send)(void* param, const struct cstring_t* protocol, const struct cstring_t* url, const struct cstring_t *received, int rport, const void* data, int bytes);

	/// @param[in] expires in seconds. if not provided, default equal to 60
	int (*onregister)(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, const char* user, const char* location, int expires);

	/// @param[in] dialog nil-new invite, not nil-reinvite
	/// @param[out] session user-defined session-id(valid only code=2xx)
    /// @return 0-ok, other-error
    int (*oninvite)(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, struct sip_dialog_t* dialog, const void* data, int bytes, void** session);
	/// @param[in] code 0-ok, other-sip status code
    int (*onack)(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session, struct sip_dialog_t* dialog, int code, const void* data, int bytes);
	/// @param[in] session oninvite return value
	int (*onprack)(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session, struct sip_dialog_t* dialog, const void* data, int bytes);
	int (*onupdate)(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session, struct sip_dialog_t* dialog, const void* data, int bytes);
    int (*oninfo)(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session, struct sip_dialog_t* dialog, const struct cstring_t* package, const void* data, int bytes);
	/// on terminating a session(dialog)
	int (*onbye)(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session);
	/// cancel a transaction(should be an invite transaction)
	int (*oncancel)(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session);

	int (*onsubscribe)(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, struct sip_subscribe_t* subscribe, void** sub);
	int (*onnotify)(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* sub, const struct cstring_t* event);
	int (*onpublish)(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, const struct cstring_t* event);

	int (*onmessage)(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session, const void* data, int bytes);
	int (*onrefer)(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session);
};

struct sip_agent_t* sip_agent_create(struct sip_uas_handler_t* handler);
int sip_agent_destroy(struct sip_agent_t* sip);

/// @param[in] msg sip request/response message
/// @param[in] param user-defined parameter, for sip_uas_handler_t only
int sip_agent_input(struct sip_agent_t* sip, struct sip_message_t* msg, void* param);

int sip_agent_set_rport(struct sip_message_t* msg, const char* peer, int port);

#if defined(__cplusplus)
}
#endif
#endif /* !_sip_agent_h_ */
