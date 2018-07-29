#ifndef _sip_message_h_
#define _sip_message_h_

#include "sip-header.h"
#include <stdint.h>

// 8.1.1.7 Via (p39)
#define SIP_BRANCH_PREFIX "z9hG4bK"

struct sip_message_t
{
	// request line
	union
	{
		struct sip_requestline_t c;
		struct sip_statusline_t s;
	} u;
	
	// 6-headers
	struct sip_contact_t to;
	struct sip_contact_t from;
	struct sip_vias_t vias;
	struct cstring_t callid;
	struct sip_cseq_t cseq;
	int maxforwards;

	// contacts/routers
	struct sip_contacts_t contacts;
	struct sip_uris_t routers;
	struct sip_uris_t record_routers; // Record-Route

	// other headers
	struct sip_params_t headers;

	const void *payload;
	int size; // payload size in byte
};

struct sip_message_t* sip_message_create();
int sip_message_addref(struct sip_message_t*);
int sip_message_release(struct sip_message_t* msg);

struct sip_message_t* sip_message_load(const uint8_t* data, int bytes);
int sip_message_write(const struct sip_message_t* msg, uint8_t* data, int bytes);

/// @return 1-invite, 0-noninvite
int sip_message_isinvite(const struct sip_message_t* msg);

#endif /* !_sip_message_h_ */
