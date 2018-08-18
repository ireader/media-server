#ifndef _sip_dialog_h_
#define _sip_dialog_h_

#include "sip-header.h"
#include "cstring.h"
#include "darray.h"
#include "list.h"

enum {
	DIALOG_ERALY = 0,
	DIALOG_CONFIRMED,
};

struct sip_dialog_t
{
	struct list_head link;
	const void* session;

	int state; // DIALOG_ERALY/DIALOG_CONFIRMED

	char callid[128];
	struct 
	{
		uint32_t id; // local/remote sequence number
		char tag[128]; //local/remote tag
		struct sip_uri_t uri; // local/remote URI(From/To Field)
	} local, remote;
	struct sip_uri_t target; //remote target(the URI from the Contact header field of the request)
	int secure; // bool

	struct sip_uris_t routers; // route set(the list of URIs in the Record-Route header field from the request)
};

struct sip_dialog_t* sip_dialog_create(const struct sip_message_t* msg);
int sip_dialog_destroy(struct sip_dialog_t* dialog);

/// @return 1-match, 0-don't match
int sip_dialog_match(const struct sip_dialog_t* dialog, const struct cstring_t* callid, const struct cstring_t* from, const struct cstring_t* to);

struct sip_dialog_t* sip_dialog_find(struct list_head* dialogs, struct sip_message_t* req);

#endif /* !_sip_dialog_h_ */
