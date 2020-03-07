#ifndef _sip_uac_h_
#define _sip_uac_h_

#include "cstring.h"
#include "sip-agent.h"

#if defined(__cplusplus)
extern "C" {
#endif

struct sip_transport_t;
struct sip_uac_transaction_t;

/// @return user-defined session-id(valid only code=2xx)
typedef void* (*sip_uac_oninvite)(void* param, const struct sip_message_t* reply, struct sip_uac_transaction_t* t, struct sip_dialog_t* dialog, int code);
/// @return 0-ok, other-error
typedef void* (*sip_uac_onsubscribe)(void* param, const struct sip_message_t* reply, struct sip_uac_transaction_t* t, struct sip_subscribe_t* subscribe, int code);
/// @return 0-ok, other-error
typedef int (*sip_uac_onreply)(void* param, const struct sip_message_t* reply, struct sip_uac_transaction_t* t, int code);
/// @return <0-error, 0-udp, 1-tcp, other-reserved
//typedef int (*sip_uac_onsend)(void* param, const char* url, const void* data, int bytes);

/// @param[in] name such as: "Alice <sip:alice@atlanta.com>"
/// @param[in] registrar register server, such as sip:registrar.biloxi.com. can be null.
/// @param[in] seconds expires seconds
struct sip_uac_transaction_t* sip_uac_register(struct sip_agent_t* sip, const char* name, const char* registrar, int seconds, sip_uac_onreply onregister, void* param);
struct sip_uac_transaction_t* sip_uac_options(struct sip_agent_t* sip, const char* from, const char* to, sip_uac_onreply onoptins, void* param);
struct sip_uac_transaction_t* sip_uac_invite(struct sip_agent_t* sip, const char* name, const char* to, sip_uac_oninvite oninvite, void* param);
struct sip_uac_transaction_t* sip_uac_cancel(struct sip_agent_t* sip, struct sip_uac_transaction_t* invit, sip_uac_onreply oncancel, void* param);
struct sip_uac_transaction_t* sip_uac_bye(struct sip_agent_t* sip, struct sip_dialog_t* dialog, sip_uac_onreply onbye, void* param);
struct sip_uac_transaction_t* sip_uac_reinvite(struct sip_agent_t* sip, struct sip_dialog_t* dialog, sip_uac_oninvite oninvite, void* param);
/// @param[in] req100rel sip 180/183 response with Require: 100rel
struct sip_uac_transaction_t* sip_uac_prack(struct sip_agent_t* sip, const struct sip_message_t* req100rel, struct sip_dialog_t* dialog, sip_uac_onreply onreply, void* param);
struct sip_uac_transaction_t* sip_uac_update(struct sip_agent_t* sip, struct sip_dialog_t* dialog, sip_uac_onreply onreply, void* param);


/// @param[in] from valid only dialog is null
/// @param[in] to valid only dialog is null
/// @param[in] event subscribe event type
/// @param[in] expires subscribe duration in sec, 0-unsubscribe
struct sip_uac_transaction_t* sip_uac_subscribe(struct sip_agent_t* sip, const char* from, const char* to, const char* event, int expires, sip_uac_onsubscribe onsubscribe, void* param);
struct sip_uac_transaction_t* sip_uac_notify(struct sip_agent_t* sip, struct sip_subscribe_t* subscribe, const char* state, sip_uac_onreply onnotify, void* param);
/// @param[in] expires subscribe duration in ms, 0-unsubscribe
struct sip_uac_transaction_t* sip_uac_resubscribe(struct sip_agent_t* sip, struct sip_subscribe_t* subscribe, int expires, sip_uac_onsubscribe onsubscribe, void* param);
struct sip_uac_transaction_t* sip_uac_publish(struct sip_agent_t* sip, const char* from, const char* to, const char* event, sip_uac_onreply onreply, void* param);

/// @param[in] dialog exist dialog(by invite), if dialog is not null, ignore from/to parameter
struct sip_uac_transaction_t* sip_uac_info(struct sip_agent_t* sip, struct sip_dialog_t* dialog, const char* package, sip_uac_onreply oninfo, void* param);
struct sip_uac_transaction_t* sip_uac_message(struct sip_agent_t* sip, const char* from, const char* to, sip_uac_onreply onmsg, void* param);
struct sip_uac_transaction_t* sip_uac_refer(struct sip_agent_t* sip, const char* from, const char* to, sip_uac_onreply onreply, void* param);

int sip_uac_transaction_ondestroy(struct sip_uac_transaction_t* t, sip_transaction_ondestroy ondestroy, void* param);
int sip_uac_add_header(struct sip_uac_transaction_t* t, const char* name, const char* value);
int sip_uac_add_header_int(struct sip_uac_transaction_t* t, const char* name, int value);

/// @param[in] t sip uac transaction, create by sip_uac_invite/sip_uac_register/...
/// @param[in] data message payload(such as SDP), maybe NULL if don't need send anything
/// @param[in] bytes data length in byte, >=0 only
/// @param[in] transport udp/tcp transport, must be valid until on callback(maybe call many times)
/// @param[in] param transport parameter
/// @return 0-ok, other-error
int sip_uac_send(struct sip_uac_transaction_t* t, const void* data, int bytes, struct sip_transport_t* transport, void* param);

#if defined(__cplusplus)
}
#endif
#endif /* !_sip_uac_h_ */
