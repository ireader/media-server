#ifndef _sip_dialog_h_
#define _sip_dialog_h_

#include "sip-header.h"
#include "sys/atomic.h"
#include "cstring.h"
#include "list.h"
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

enum {
	DIALOG_ERALY = 0,
	DIALOG_CONFIRMED,
	DIALOG_TERMINATED,
};

struct sip_agent_t;
struct sip_message_t;
struct sip_dialog_t
{
	int state; // DIALOG_ERALY/DIALOG_CONFIRMED

	struct cstring_t callid;
	struct 
	{
		uint32_t id; // local/remote sequence number
		uint32_t rseq; // rfc3262 PRACK, [1, 2**31 - 1]
		struct sip_contact_t uri; // local/remote URI(From/To Field)
		struct sip_uri_t target; // local/remote target(Contact Field)
	} local, remote;
	int secure; // bool

	// route set(the list of URIs in the Record-Route header field from the request)
	struct sip_uris_t routers;

	// internal use only
	void* session; // user-defined session
	void (*ondestroy)(void* param);
	void* ondestroyparam;
	struct list_head link;
	char* ptr;
	int32_t ref;
};

struct sip_dialog_t* sip_dialog_create(void);
int sip_dialog_release(struct sip_dialog_t* dialog);
int sip_dialog_addref(struct sip_dialog_t* dialog);
    
int sip_dialog_init_uac(struct sip_dialog_t* dialog, const struct sip_message_t* msg);
int sip_dialog_init_uas(struct sip_dialog_t* dialog, const struct sip_message_t* msg);

int sip_dialog_setlocaltag(struct sip_dialog_t* dialog, const struct cstring_t* tag);
int sip_dialog_target_refresh(struct sip_dialog_t* dialog, const struct sip_message_t* msg);
int sip_dialog_set_local_target(struct sip_dialog_t* dialog, const struct sip_message_t* msg);

// dialog management
int sip_dialog_add(struct sip_agent_t* sip, struct sip_dialog_t* dialog);
int sip_dialog_remove(struct sip_agent_t* sip, struct sip_dialog_t* dialog);

/// call sip_dialog_release
struct sip_dialog_t* sip_dialog_fetch(struct sip_agent_t* sip, const struct cstring_t* callid, const struct cstring_t* local, const struct cstring_t* remote);

int sip_dialog_ondestroy(struct sip_dialog_t* dialog, void (*ondestroy)(void* param), void* param);

#if defined(__cplusplus)
}
#endif
#endif /* !_sip_dialog_h_ */
