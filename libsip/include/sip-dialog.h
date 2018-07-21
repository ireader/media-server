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

	int state; // DIALOG_ERALY/DIALOG_CONFIRMED

	struct cstring_t callid;
	struct 
	{
		uint32_t id; // local/remote sequence number
		struct cstring_t tag; //local/remote tag
		struct sip_uri_t uri; // local/remote URI
	} local, remote;
	struct sip_uri_t target; //remote target(the URI from the Contact header field of the request)
	int secure; // bool

	struct sip_uris_t routers; // route set(the list of URIs in the Record-Route header field from the request)
};

#endif /* !_sip_dialog_h_ */
