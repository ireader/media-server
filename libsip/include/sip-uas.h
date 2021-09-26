#ifndef _sip_uas_h_
#define _sip_uas_h_

#include "cstring.h"
#include "sip-agent.h"

#if defined(__cplusplus)
extern "C" {
#endif

struct sip_uas_transaction_t;

int sip_uas_add_header(struct sip_uas_transaction_t* t, const char* name, const char* value);
int sip_uas_add_header_int(struct sip_uas_transaction_t* t, const char* name, int value);

int sip_uas_reply(struct sip_uas_transaction_t* t, int code, const void* data, int bytes, void* param);

int sip_uas_transaction_addref(struct sip_uas_transaction_t* t);
int sip_uas_transaction_release(struct sip_uas_transaction_t* t);

int sip_uas_transaction_ondestroy(struct sip_uas_transaction_t* t, sip_transaction_ondestroy ondestroy, void* param);

#if defined(__cplusplus)
}
#endif
#endif /* !_sip_uas_h_ */
