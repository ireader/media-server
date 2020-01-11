#include "sys/sock.h"
#include "sip-uas.h"
#include "sip-header.h"
#include "sip-dialog.h"
#include "sip-message.h"

static struct sip_dialog_t* s_dialog;

static struct sip_message_t* req2sip(const char* req)
{
	struct sip_message_t* msg;
	msg = sip_message_create(SIP_MESSAGE_REQUEST);

	size_t n = strlen(req);
	http_parser_t* parser = http_parser_create(HTTP_PARSER_REQUEST, NULL, NULL);
	assert(0 == http_parser_input(parser, req, &n) && 0 == n);
	assert(0 == sip_message_load(msg, parser));
	http_parser_destroy(parser);
	return msg;
}

static struct sip_message_t* reply2sip(const char* reply)
{
	struct sip_message_t* msg;
	msg = sip_message_create(SIP_MESSAGE_REPLY);

	size_t n = strlen(reply);
	http_parser_t* parser = http_parser_create(HTTP_PARSER_RESPONSE, NULL, NULL);
	assert(0 == http_parser_input(parser, reply, &n) && 0 == n);
	assert(0 == sip_message_load(msg, parser));
	http_parser_destroy(parser);
	return msg;
}

static int sip_register(struct sip_agent_t* sip)
{
	// F1 REGISTER Bob -> Registrar (p213)
	const char* f1 = "REGISTER sip:registrar.biloxi.com SIP/2.0\r\n"
		"Via: SIP/2.0/UDP bobspc.biloxi.com:5060;branch=z9hG4bKnashds7;received=192.0.2.4\r\n"
		"Max-Forwards: 70\r\n"
		"To: Bob <sip:bob@biloxi.com>\r\n"
		"From: Bob <sip:bob@biloxi.com>;tag=456248\r\n"
		"Call-ID: 843817637684230@998sdasdh09\r\n"
		"CSeq: 1826 REGISTER\r\n"
		"Contact: <sip:bob@192.0.2.4>\r\n"
		"Expires: 7200\r\n"
		"Content-Length: 0\r\n\r\n";

	// F2 200 OK Registrar -> Bob (p214)
	const char* f2 = "SIP/2.0 200 OK\r\n"
		"Via: SIP/2.0/UDP bobspc.biloxi.com:5060;branch=z9hG4bKnashds7;received=192.0.2.4\r\n"
		"To: Bob <sip:bob@biloxi.com>;tag=2493k59kd\r\n"
		"From: Bob <sip:bob@biloxi.com>;tag=456248\r\n"
		"Call-ID: 843817637684230@998sdasdh09\r\n"
		"CSeq: 1826 REGISTER\r\n"
		"Contact: <sip:bob@192.0.2.4>\r\n"
		"Expires: 7200\r\n"
		"Content-Length: 0\r\n\r\n";

	struct sip_message_t* req = req2sip(f1);
	struct sip_message_t* reply = reply2sip(f2);
	assert(0 == sip_agent_input(sip, req));
	sip_message_destroy(req);
	sip_message_destroy(reply);

	return 0;
}

static int sip_invite(struct sip_agent_t* sip)
{
	// F1 INVITE Alice -> atlanta.com proxy (p214)
	const char* f1 = "INVITE sip:bob@biloxi.com SIP/2.0\r\n"
		"Via: SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bKnashds8\r\n"
		"Max-Forwards: 70\r\n"
		//"To: Bob <sip:bob@biloxi.com>\r\n"
		"To: Bob <sip:bob@biloxi.com>;tag=a6c85cf\r\n"
		"From: Alice <sip:alice@atlanta.com>;tag=1928301774\r\n"
		"Call-ID: a84b4c76e66710\r\n"
		"CSeq: 314159 INVITE\r\n"
		"Contact: <sip:alice@pc33.atlanta.com>\r\n"
		"Content-Type: application/sdp\r\n"
		"Content-Length: 0\r\n\r\n";

	// F2 100 Trying atlanta.com proxy -> Alice (p215)
	const char* f2 = "SIP/2.0 100 Trying\r\n"
		"Via: SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bKnashds8;received=192.0.2.1\r\n"
		"To: Bob <sip:bob@biloxi.com>\r\n"
		"From: Alice <sip:alice@atlanta.com>;tag=1928301774\r\n"
		"Call-ID: a84b4c76e66710\r\n"
		"CSeq: 314159 INVITE\r\n"
		"Content-Length: 0\r\n\r\n";

	// F8 180 Ringing atlanta.com proxy -> Alice (217)
	const char* f8 = "SIP/2.0 180 Ringing\r\n"
		"Via: SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bKnashds8;received=192.0.2.1\r\n"
		"To: Bob <sip:bob@biloxi.com>;tag=a6c85cf\r\n"
		"From: Alice <sip:alice@atlanta.com>;tag=1928301774\r\n"
		"Call-ID: a84b4c76e66710\r\n"
		"Contact: <sip:bob@192.0.2.4>\r\n"
		"CSeq: 314159 INVITE\r\n"
		"Content-Length: 0\r\n\r\n";

	// F11 200 OK atlanta.com proxy -> Alice (p218)
	const char* f11 = "SIP/2.0 200 OK\r\n"
		"Via: SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bKnashds8;received=192.0.2.1\r\n"
		"To: Bob <sip:bob@biloxi.com>;tag=a6c85cf\r\n"
		"From: Alice <sip:alice@atlanta.com>;tag=1928301774\r\n"
		"Call-ID: a84b4c76e66710\r\n"
		"CSeq: 314159 INVITE\r\n"
		"Contact: <sip:bob@192.0.2.4>\r\n"
		"Content-Type: application/sdp\r\n"
		"Content-Length: 0\r\n\r\n";

	// F12 ACK Alice -> Bob (p218)
	const char* f12 = "ACK sip:bob@192.0.2.4 SIP/2.0\r\n"
		"Via: SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bKnashds9\r\n"
		"Max-Forwards: 70\r\n"
		"To: Bob <sip:bob@biloxi.com>;tag=a6c85cf\r\n"
		"From: Alice <sip:alice@atlanta.com>;tag=1928301774\r\n"
		"Call-ID: a84b4c76e66710\r\n"
		"CSeq: 314159 ACK\r\n"
		"Content-Length: 0\r\n\r\n";

	struct sip_message_t* req = req2sip(f1);
	struct sip_message_t* reply100 = reply2sip(f2);
	struct sip_message_t* reply180 = reply2sip(f8);
	struct sip_message_t* reply200 = reply2sip(f11);
	struct sip_message_t* ack = req2sip(f12);

	assert(0 == sip_agent_input(sip, req));
	assert(0 == sip_agent_input(sip, req));
	assert(0 == sip_agent_input(sip, req));
	assert(0 == sip_agent_input(sip, ack));
	assert(0 == sip_agent_input(sip, ack));
	assert(0 == sip_agent_input(sip, ack));
	
	sip_message_destroy(req);
	sip_message_destroy(reply100);
	sip_message_destroy(reply180);
	sip_message_destroy(reply200);
	sip_message_destroy(ack);

	return 0;
}

static int sip_bye(struct sip_agent_t* sip)
{
	//// F13 BYE Bob -> Alice (p218)
	//const char* f13 = "BYE sip:alice@pc33.atlanta.com SIP/2.0\r\n"
	//	"Via: SIP/2.0/UDP 192.0.2.4;branch=z9hG4bKnashds10\r\n"
	//	"Max-Forwards: 70\r\n"
	//	"From: Bob <sip:bob@biloxi.com>;tag=a6c85cf\r\n"
	//	"To: Alice <sip:alice@atlanta.com>;tag=1928301774\r\n"
	//	"Call-ID: a84b4c76e66710\r\n"
	//	"CSeq: 231 BYE\r\n"
	//	"Content-Length: 0\r\n\r\n";

	//// F14 200 OK Alice -> Bob (p219)
	//const char* f14 = "SIP/2.0 200 OK\r\n"
	//	"Via: SIP/2.0/UDP 192.0.2.4;branch=z9hG4bKnashds10\r\n"
	//	"From: Bob <sip:bob@biloxi.com>;tag=a6c85cf\r\n"
	//	"To: Alice <sip:alice@atlanta.com>;tag=1928301774\r\n"
	//	"Call-ID: a84b4c76e66710\r\n"
	//	"CSeq: 231 BYE\r\n"
	//	"Content-Length: 0\r\n\r\n";

	// F13 BYE Alice -> Bob (p218)
	const char* f13 = "BYE sip:bob@192.0.2.4 SIP/2.0\r\n"
		"Via: SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bKnashds9\r\n"
		"Max-Forwards: 70\r\n"
		"To: Bob <sip:bob@biloxi.com>;tag=a6c85cf\r\n"
		"From: Alice <sip:alice@atlanta.com>;tag=1928301774\r\n"
		"Call-ID: a84b4c76e66710\r\n"
		"CSeq: 314159 BYE\r\n"
		"Content-Length: 0\r\n\r\n";

	// F14 200 OK Alice -> Bob (p219)
	const char* f14 = "SIP/2.0 200 OK\r\n"
		"Via: SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bKnashds9\r\n"
		"To: Bob <sip:bob@biloxi.com>;tag=a6c85cf\r\n"
		"From: Alice <sip:alice@atlanta.com>;tag=1928301774\r\n"
		"Call-ID: a84b4c76e66710\r\n"
		"CSeq: 314159 BYE\r\n"
		"Content-Length: 0\r\n\r\n";

	struct sip_message_t* req = req2sip(f13);
	struct sip_message_t* reply = reply2sip(f14);
	assert(0 == sip_agent_input(sip, req));
	sip_message_destroy(req);
	sip_message_destroy(reply);

	return 0;
}

struct sip_session_t
{
	struct sip_uas_transaction_t* t;
	struct sip_dialog_t* dialog;	
};

static void* sip_uas_oninvite(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, struct sip_dialog_t* dialog, const void* data, int bytes)
{
	struct sip_session_t* session = new struct sip_session_t;
	assert(NULL == dialog); // re-invite
	//sip_uas_add_header(t, "To", "Bob <sip:bob@biloxi.com>;tag=a6c85cf");
	assert(0 == sip_uas_reply(t, 100, NULL, 0));
	assert(0 == sip_uas_reply(t, 100, NULL, 0));
	assert(0 == sip_uas_reply(t, 180, NULL, 0));
	assert(0 == sip_uas_reply(t, 180, NULL, 0));
	assert(0 == sip_uas_reply(t, 180, NULL, 0));
	assert(0 == sip_uas_reply(t, 200, NULL, 0));
//	assert(0 == sip_uas_reply(t, 200, NULL, 0));
	session->t = t;
	return session;
}

static int sip_uas_onack(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session, struct sip_dialog_t* dialog, int code, const void* data, int bytes)
{
	char buf[1024];
	const char* end = buf + sizeof(buf);
	struct cstring_t ptr;
	ptr.p = buf;

	struct sip_session_t* s = (struct sip_session_t*)session; 
	struct sip_agent_t* uas = *(struct sip_agent_t**)param;
	assert(100 <= code && code < 700);
	if (200 <= code && code < 300)
	{
		assert(dialog->state == DIALOG_CONFIRMED);
		assert(dialog->secure == 0);
		assert(dialog->remote.id == 314159);
		ptr.n = sip_contact_write(&dialog->local.uri, buf, end);
		assert(0 == cstrcmp(&ptr, "Bob <sip:bob@biloxi.com>;tag=a6c85cf"));
		ptr.n = sip_contact_write(&dialog->remote.uri, buf, end);
		assert(0 == cstrcmp(&ptr, "Alice <sip:alice@atlanta.com>;tag=1928301774"));
		ptr.n = sip_uri_write(&dialog->target, buf, end);
		assert(0 == cstrcmp(&ptr, "sip:alice@pc33.atlanta.com"));
		assert(0 == cstrcmp(&dialog->callid, "a84b4c76e66710"));
		assert(0 == sip_uris_count(&dialog->routers));
		s->dialog = dialog;
	}
	else if (300 <= code && code < 700)
	{
		delete s;
	}
	return 0;
}

static int sip_uas_onbye(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session)
{
	struct sip_session_t* s = (struct sip_session_t*)session;
	struct sip_agent_t* uas = *(struct sip_agent_t**)param;
	assert(0 == sip_uas_reply(t, 200, NULL, 0));
	if(s) delete s;
	return 0;
}

static int sip_uas_oncancel(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session)
{
	assert(0);
	return -1;
}

static int sip_uas_onregister(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, const char* user, const char* location, int expires)
{
	assert(expires == 7200);
	assert(0 == strcmp(user, "bob"));
	assert(0 == strcmp(location, "192.0.2.4"));
	struct sip_agent_t* uas = *(struct sip_agent_t**)param;
	return sip_uas_reply(t, 200, NULL, 0);
}

static int sip_uas_onmessage(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session, const void* payload, int bytes)
{
	return sip_uas_reply(t, 200, NULL, 0);
}

static int sip_uas_send(void* param, const struct cstring_t* url, const void* data, int bytes)
{
	((char*)data)[bytes] = 0;
	((char*)url->p)[url->n] = 0;
	printf("==> %s\n%s\n", url->p, (const char*)data);
	return 0;
}

// 24 Examples (p213)
void sip_uas_message_test(void)
{
	struct sip_uas_handler_t handler;
	handler.onregister = sip_uas_onregister;
	handler.oninvite = sip_uas_oninvite;
	handler.onack = sip_uas_onack;
	handler.onbye = sip_uas_onbye;
	handler.oncancel = sip_uas_oncancel;
	handler.onmessage = sip_uas_onmessage;
	handler.send = sip_uas_send;

	struct sip_agent_t* sip;
	sip = sip_agent_create(&handler, &sip);
	assert(0 == sip_register(sip));
	assert(0 == sip_invite(sip));
	assert(0 == sip_bye(sip));
	sip_agent_destroy(sip);
}
