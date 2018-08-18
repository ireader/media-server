#ifndef _sip_uac_h_
#define _sip_uac_h_

#include "sip-message.h"

struct sip_uac_t;
struct sip_uac_transaction_t;

typedef int (*sip_uac_oninvite)(void* param, struct sip_uac_transaction_t* t, struct sip_dialog_t* dialog, int code);
typedef int (*sip_uac_onreply)(void* param, struct sip_uac_transaction_t* t, int code);
typedef int (*sip_uac_onsend)(void* param, const char* host, int port, const void* data, int bytes);

/// @param[in] name such as: "Alice <sip:alice@atlanta.com>"
struct sip_uac_t* sip_uac_create();
int sip_uac_destroy(struct sip_uac_t* uac);

int sip_uac_input(struct sip_uac_t* uac, struct sip_message_t* reply);

struct sip_uac_transaction_t* sip_uac_invite(struct sip_uac_t* uac, const char* name, const char* to, sip_uac_oninvite* oninvite, void* param);
struct sip_uac_transaction_t* sip_uac_cancel(struct sip_uac_t* uac, void* session, sip_uac_onreply oncancel, void* param);
struct sip_uac_transaction_t* sip_uac_bye(struct sip_uac_t* uac, void* session, sip_uac_onreply onbye, void* param);
struct sip_uac_transaction_t* sip_uac_reinvite(struct sip_uac_t* uac, void* session, sip_uac_oninvite* oninvite, void* param);
/// @param[in] seconds expires seconds
struct sip_uac_transaction_t* sip_uac_register(struct sip_uac_t* uac, const char* name, int seconds, sip_uac_onreply onregister, void* param);
struct sip_uac_transaction_t* sip_uac_options(struct sip_uac_t* uac, const char* name, const char* to, sip_uac_onreply onoptins, void* param);

// valid only on callback
int sip_uac_get_header_count(struct sip_uac_transaction_t* t);
int sip_uac_get_header(struct sip_uac_transaction_t* t, int i, const char** name, const char** value);
const char* sip_uac_get_header_by_name(struct sip_uac_transaction_t* t, const char* name);

int sip_uac_add_header(struct sip_uac_transaction_t* t, const char* name, const char* value);

int sip_uac_send(struct sip_uac_transaction_t* t, const void* data, int bytes, sip_uac_onsend onsend, void* param);

#endif /* !_sip_uac_h_ */
