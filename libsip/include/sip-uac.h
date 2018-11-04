#ifndef _sip_uac_h_
#define _sip_uac_h_

#include "cstring.h"

#if defined(__cplusplus)
extern "C" {
#endif

struct sip_uac_t;
struct sip_dialog_t;
struct sip_message_t;
struct sip_transport_t;
struct sip_uac_transaction_t;

/// @return 0-ok, other-error
typedef int (*sip_uac_oninvite)(void* param, const struct sip_message_t* reply, struct sip_uac_transaction_t* t, struct sip_dialog_t* dialog, int code);
/// @return 0-ok, other-error
typedef int (*sip_uac_onreply)(void* param, const struct sip_message_t* reply, struct sip_uac_transaction_t* t, int code);
/// @return <0-error, 0-udp, 1-tcp, other-reserved
//typedef int (*sip_uac_onsend)(void* param, const char* url, const void* data, int bytes);

/// @param[in] name such as: "Alice <sip:alice@atlanta.com>"
struct sip_uac_t* sip_uac_create();
int sip_uac_destroy(struct sip_uac_t* uac);

int sip_uac_input(struct sip_uac_t* uac, struct sip_message_t* reply);

/// @param[in] registrar register server, such as sip:registrar.biloxi.com. can be null.
/// @param[in] seconds expires seconds
struct sip_uac_transaction_t* sip_uac_register(struct sip_uac_t* uac, const char* name, const char* registrar, int seconds, sip_uac_onreply onregister, void* param);
struct sip_uac_transaction_t* sip_uac_options(struct sip_uac_t* uac, const char* from, const char* to, sip_uac_onreply onoptins, void* param);
struct sip_uac_transaction_t* sip_uac_invite(struct sip_uac_t* uac, const char* name, const char* to, sip_uac_oninvite oninvite, void* param);
struct sip_uac_transaction_t* sip_uac_cancel(struct sip_uac_t* uac, struct sip_uac_transaction_t* invit, sip_uac_onreply oncancel, void* param);
struct sip_uac_transaction_t* sip_uac_bye(struct sip_uac_t* uac, struct sip_dialog_t* dialog, sip_uac_onreply onbye, void* param);
struct sip_uac_transaction_t* sip_uac_reinvite(struct sip_uac_t* uac, struct sip_dialog_t* dialog, sip_uac_oninvite oninvite, void* param);
struct sip_uac_transaction_t* sip_uac_info(struct sip_uac_t* uac, const char* from, const char* to, sip_uac_onreply oninfo, void* param);
struct sip_uac_transaction_t* sip_uac_message(struct sip_uac_t* uac, const char* from, const char* to, sip_uac_onreply onmsg, void* param);
struct sip_uac_transaction_t* sip_uac_subscribe(struct sip_uac_t* uac, const char* from, const char* to, sip_uac_onreply onsubscribe, void* param);
struct sip_uac_transaction_t* sip_uac_notify(struct sip_uac_t* uac, const char* from, const char* to, sip_uac_onreply onnotify, void* param);
struct sip_uac_transaction_t* sip_uac_custom(struct sip_uac_t* uac, const char* method, const char* from, const char* to, sip_uac_onreply onreply, void* param);

int sip_uac_add_header(struct sip_uac_transaction_t* t, const char* name, const char* value);
int sip_uac_add_header_int(struct sip_uac_transaction_t* t, const char* name, int value);

/// @param[in] t sip uac transaction, create by sip_uac_invite/sip_uac_register/...
/// @param[in] data message payload(such as SDP), maybe NULL if don't need send anything
/// @param[in] bytes data length in byte, >=0 only
/// @param[in] onsend udp/tcp transport, must be valid until on callback(maybe call many times)
/// @return 0-ok, other-error
int sip_uac_send(struct sip_uac_transaction_t* t, const void* data, int bytes, struct sip_transport_t* transport, void* param);

#if defined(__cplusplus)
}
#endif
#endif /* !_sip_uac_h_ */
