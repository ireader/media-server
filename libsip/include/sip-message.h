#ifndef _sip_message_h_
#define _sip_message_h_

#include "sip-header.h"
#include "sip-dialog.h"
#include "http-parser.h"
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

// 8.1.1.7 Via (p39)
#define SIP_BRANCH_PREFIX		"z9hG4bK"
#define SIP_MAX_FORWARDS		70

// https://en.wikipedia.org/wiki/List_of_SIP_request_methods
#define SIP_METHOD_INVITE		"INVITE"
#define SIP_METHOD_CANCEL		"CANCEL"
#define SIP_METHOD_BYE			"BYE"
#define SIP_METHOD_ACK			"ACK"
#define SIP_METHOD_OPTIONS		"OPTIIONS"
#define SIP_METHOD_REGISTER		"REGISTER"
#define SIP_METHOD_PRACK		"PRACK" // rfc3262
#define SIP_METHOD_INFO			"INFO" // rfc2976/rfc6086
#define SIP_METHOD_REFER		"REFER" // rfc3515
#define SIP_METHOD_MESSAGE		"MESSAGE" // rfc3248
#define SIP_METHOD_SUBSCRIBE	"SUBSCRIBE" // rfc4660/rfc6665
#define SIP_METHOD_NOTIFY		"NOTIFY" // rfc4660/rfc6665
#define SIP_METHOD_PUBLISH		"PUBLISH" // rfc3903
#define SIP_METHOD_UPDATE		"UPDATE" // rfc3311

#define SIP_HEADER_FROM			"From"
#define SIP_HEADER_TO			"To"
#define SIP_HEADER_CALLID		"Call-ID"
#define SIP_HEADER_CSEQ			"CSeq"
#define SIP_HEADER_MAX_FORWARDS	"Max-Forwards"
#define SIP_HEADER_VIA			"Via"
#define SIP_HEADER_CONTACT		"Contact"
#define SIP_HEADER_ROUTE		"Route"
#define SIP_HEADER_RECORD_ROUTE "Record-Route"
#define SIP_HEADER_RSEQ			"RSeq" // rfc3262
#define SIP_HEADER_RACK			"RAck" // rfc3262
#define SIP_HEADER_REFER_TO		"Refer-To" // rfc3515
#define SIP_HEADER_RECV_INFO	"Recv-Info" // rfc2976/rfc6086
#define SIP_HEADER_INFO_PACKAGE	"Info-Package" // rfc2976/rfc6086
#define SIP_HEADER_EVENT		"Event" // rfc3265/rfc6665
#define SIP_HEADER_ALLOW_EVENTS	"Allow-Events" // rfc3265/rfc6665
#define SIP_HEADER_SUBSCRIBE_STATE "Subscription-State" // rfc3265/rfc6665

#define SIP_HEADER_ABBR_FROM				"f"
#define SIP_HEADER_ABBR_TO					"t"
#define SIP_HEADER_ABBR_CALLID				"i"
#define SIP_HEADER_ABBR_VIA					"v"
#define SIP_HEADER_ABBR_CONTACT				"m"
#define SIP_HEADER_ABBR_SUPPORTED			"k"
#define SIP_HEADER_ABBR_SUBJECT				"s"
#define SIP_HEADER_ABBR_CONTENT_TYPE		"c"
#define SIP_HEADER_ABBR_CONTENT_LENGTH		"l"
#define SIP_HEADER_ABBR_CONTENT_ENCODING	"e"
#define SIP_HEADER_ABBR_REFER_TO			"r"


#define SIP_OPTION_TAG_100REL	"100rel"  // rfc3262

enum { SIP_MESSAGE_REQUEST = 0, SIP_MESSAGE_REPLY = 1 };
struct sip_message_t
{
	// request line
	int mode; // SIP_MESSAGE_REQUEST/SIP_MESSAGE_REPLY
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
	uint32_t rseq; // [1, 2**31 - 1] PRACK
	struct cstring_t recv_info; // Info Method (invite)
	struct cstring_t info_package; // Info Method
	struct sip_contact_t referto; // Refer Method
	struct cstring_t event; // Subscribe/Notify Method
	struct cstring_t allow_events; // Subscribe/Notify Method
	struct sip_substate_t substate; // Subscribe/Notify Method (invite)
	struct sip_params_t headers;

	const void *payload;
	int size; // payload size in byte

	// internal use only
	struct
	{
		char* ptr;
		char* end;
	} ptr;
};

struct sip_message_t* sip_message_create(int mode);
int sip_message_destroy(struct sip_message_t* msg);
int sip_message_clone(struct sip_message_t* msg, const struct sip_message_t* clone);
int sip_message_init(struct sip_message_t* msg, const char* method, const char* uri, const char* from, const char* to);
int sip_message_init2(struct sip_message_t* msg, const char* method, const struct sip_dialog_t* dialog);
int sip_message_init3(struct sip_message_t* reply, const struct sip_message_t* req);
int sip_message_initack(struct sip_message_t* ack, const struct sip_message_t* origin);

int sip_message_load(struct sip_message_t* msg, const struct http_parser_t* parser);
int sip_message_write(const struct sip_message_t* msg, uint8_t* data, int bytes);

/// @return 1-ack, 0-not ack
int sip_message_isack(const struct sip_message_t* msg);
int sip_message_isbye(const struct sip_message_t* msg);
int sip_message_iscancel(const struct sip_message_t* msg);
/// @return 1-invite, 0-noninvite
int sip_message_isinvite(const struct sip_message_t* msg);
int sip_message_isregister(const struct sip_message_t* msg);
int sip_message_isrefer(const struct sip_message_t* msg);
int sip_message_isnotify(const struct sip_message_t* msg);
int sip_message_issubscribe(const struct sip_message_t* msg);

int sip_message_set_uri(struct sip_message_t* msg, const char* uri);
const struct sip_uri_t* sip_message_get_next_hop(const struct sip_message_t* msg);
int sip_message_set_reply_default_contact(struct sip_message_t* reply);

int sip_message_get_header_count(const struct sip_message_t* msg);
int sip_message_get_header(const struct sip_message_t* msg, int i, struct cstring_t* const name, struct cstring_t* const value);
const struct cstring_t* sip_message_get_header_by_name(const struct sip_message_t* msg, const char* name);

int sip_message_add_header(struct sip_message_t* msg, const char* name, const char* value);
int sip_message_add_header_int(struct sip_message_t* msg, const char* name, int value);

#if defined(__cplusplus)
}
#endif
#endif /* !_sip_message_h_ */
