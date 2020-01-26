#include "sys/sock.h"
#include "sip-uas.h"
#include "sip-header.h"
#include "sip-dialog.h"
#include "sip-message.h"
#include "sip-transport.h"
extern "C" {
#include "../src/uac/sip-uac-transaction.h"
}
#include "port/ip-route.h"
#include "http-parser.h"
#include "uri-parse.h"

struct sip_message_test_t
{
	struct sip_agent_t* sip;
	struct sip_transport_t udp;
};

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

static void* sip_uac_test_oninvite(void* param, const struct sip_message_t* reply, struct sip_uac_transaction_t* t, struct sip_dialog_t* dialog, int code)
{
	return NULL;
}

static void* sip_uac_test_onsubscribe(void* param, const struct sip_message_t* reply, struct sip_uac_transaction_t* t, struct sip_subscribe_t* subscribe, int code)
{
	return NULL;
}

static int sip_uac_test_onreply(void* param, const struct sip_message_t* reply, struct sip_uac_transaction_t* t, int code)
{
	return 0;
}

static inline struct sip_uac_transaction_t* sip_uac_transaction_create1(struct sip_agent_t* sip, struct sip_message_t* req)
{
	struct sip_uac_transaction_t* t;
	t = sip_uac_transaction_create(sip, req);
	t->onreply = sip_uac_test_onreply;
	t->param = NULL;
	return t;
}
static inline struct sip_uac_transaction_t* sip_uac_transaction_create2(struct sip_agent_t* sip, struct sip_message_t* req)
{
	struct sip_uac_transaction_t* t;
	t = sip_uac_transaction_create(sip, req);
	t->oninvite = sip_uac_test_oninvite;
	t->param = NULL;
	return t;
}
static inline struct sip_uac_transaction_t* sip_uac_transaction_create3(struct sip_agent_t* sip, struct sip_message_t* req)
{
	struct sip_uac_transaction_t* t;
	t = sip_uac_transaction_create(sip, req);
	t->onsubscribe = sip_uac_test_onsubscribe;
	t->param = NULL;
	return t;
}

static int sip_uac_transport_via(void* transport, const char* destination, char protocol[16], char local[128], char dns[128])
{
	int r;
	char ip[65];
	u_short port;
	struct uri_t* uri;
	struct sockaddr_storage ss;
	socklen_t len;

	len = sizeof(ss);
	memset(&ss, 0, sizeof(ss));
	strcpy(protocol, "UDP");

	uri = uri_parse(destination, strlen(destination));
	if (!uri)
		return -1; // invalid uri

	// TODO: sips port
	r = socket_addr_from(&ss, &len, "atlanta.com" /*uri->host*/, uri->port ? uri->port : SIP_PORT);
	if (0 == r)
	{
		socket_addr_to((struct sockaddr*)&ss, len, ip, &port);
		r = ip_route_get(ip, local);
		if (0 == r)
		{
			len = sizeof(ss);
			if (0 == socket_addr_from(&ss, &len, local, 0))
				socket_addr_name((struct sockaddr*)&ss, len, dns, 128);

			if (SIP_PORT != port)
				snprintf(local + strlen(local), 128 - strlen(local), ":%hu", port);

			if (NULL == strchr(dns, '.'))
				snprintf(dns, 128, "%s", local); // don't have valid dns
		}
	}

	uri_free(uri);
	return r;
}

static int sip_uac_transport_send(void* transport, const void* data, size_t bytes)
{
	printf("%.*s\n", (int)bytes, (const char*)data);
	return 0;
}

static int sip_uas_send(void* param, const struct cstring_t* url, const void* data, int bytes)
{
	printf("==> %.*s\n%.*s\n", (int)url->n, url->p, (int)bytes, (const char*)data);
	return 0;
}

static int sip_test_register(struct sip_message_test_t* alice, struct sip_message_test_t *bob)
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
	assert(0 == sip_uac_send(sip_uac_transaction_create1(bob->sip, req), NULL, 0, &bob->udp, &bob));
	assert(0 == sip_agent_input(alice->sip, req));
	assert(0 == sip_agent_input(bob->sip, reply));
	//sip_message_destroy(req); // delete by uac transaction
	sip_message_destroy(reply);

	return 0;
}

static int sip_test_invite(struct sip_message_test_t* alice, struct sip_message_test_t *bob)
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

	const char* f13 = "SIP/2.0 603 Decline\r\n"
		"Via: SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bKnashds8;received=192.0.2.1\r\n"
		"To: Bob <sip:bob@biloxi.com>;tag=a6c85cf\r\n"
		"From: Alice <sip:alice@atlanta.com>;tag=1928301774\r\n"
		"Call-ID: a84b4c76e66710\r\n"
		"CSeq: 314159 INVITE\r\n"
		"Content-Type: application/sdp\r\n"
		"Content-Length: 0\r\n\r\n";

	struct sip_message_t* req = req2sip(f1);
	struct sip_message_t* reply100 = reply2sip(f2);
	struct sip_message_t* reply180 = reply2sip(f8);
	struct sip_message_t* reply200 = reply2sip(f11);
	struct sip_message_t* reply603 = reply2sip(f13);
	struct sip_message_t* ack = req2sip(f12);

	assert(0 == sip_uac_send(sip_uac_transaction_create2(alice->sip, req), NULL, 0, &alice->udp, &alice));
	assert(0 == sip_agent_input(bob->sip, req));
	assert(0 == sip_agent_input(bob->sip, req));
	assert(0 == sip_agent_input(bob->sip, req));
	assert(0 == sip_agent_input(bob->sip, req));
	assert(0 == sip_agent_input(alice->sip, reply100));
	assert(0 == sip_agent_input(alice->sip, reply180));
	assert(0 == sip_agent_input(alice->sip, reply100));
	assert(0 == sip_agent_input(alice->sip, reply180));
//	assert(0 == sip_agent_input(alice->sip, reply603));
	assert(0 == sip_agent_input(alice->sip, reply200));
	assert(0 == sip_agent_input(alice->sip, reply180));
	assert(0 == sip_agent_input(alice->sip, reply100));
	assert(0 == sip_agent_input(alice->sip, reply200));
	assert(0 == sip_agent_input(bob->sip, ack));
	assert(0 == sip_agent_input(bob->sip, ack));
	assert(0 == sip_agent_input(bob->sip, ack));

	//sip_message_destroy(req); // delete by uac transaction
	sip_message_destroy(reply100);
	sip_message_destroy(reply180);
	sip_message_destroy(reply200);
	sip_message_destroy(reply603);
	sip_message_destroy(ack);

	return 0;
}

static int sip_test_bye(struct sip_message_test_t* alice, struct sip_message_test_t *bob)
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
	assert(0 == sip_uac_send(sip_uac_transaction_create1(alice->sip, req), NULL, 0, &alice->udp, &alice));
	assert(0 == sip_agent_input(bob->sip, req));
	assert(0 == sip_agent_input(alice->sip, reply));
	//sip_message_destroy(req); // delete by uac transaction
	sip_message_destroy(reply);

	return 0;
}

static void* sip_uas_oninvite(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, struct sip_dialog_t* dialog, const void* data, int bytes)
{
	return NULL;
}

static int sip_uas_onack(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session, struct sip_dialog_t* dialog, int code, const void* data, int bytes)
{
	return 0;
}

static int sip_uas_onbye(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session)
{
	return sip_uas_discard(t);
}

static int sip_uas_oncancel(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session)
{
	return sip_uas_discard(t);
}

static int sip_uas_onprack(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session, struct sip_dialog_t* dialog, const void* data, int bytes)
{
	return sip_uas_discard(t);
}

static int sip_uas_onupdate(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session, struct sip_dialog_t* dialog, const void* data, int bytes)
{
	return sip_uas_discard(t);
}

static int sip_uas_onregister(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, const char* user, const char* location, int expires)
{
	assert(expires == 7200);
	assert(0 == strcmp(user, "bob"));
	assert(0 == strcmp(location, "192.0.2.4"));
	struct sip_agent_t* uas = *(struct sip_agent_t**)param;
	return sip_uas_discard(t);
}

static int sip_uas_onmessage(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session, const void* payload, int bytes)
{
	return sip_uas_discard(t);
}

static void* sip_uas_onsubscribe(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, sip_subscribe_t* subscribe)
{
	sip_uas_discard(t);
	return NULL;
}

static int sip_uas_onnotify(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session, const struct cstring_t* event)
{
	return sip_uas_discard(t);
}

static int sip_uas_onpublish(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, const struct cstring_t* event)
{
	return sip_uas_discard(t);
}

static int sip_uas_oninfo(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session)
{
	return sip_uas_discard(t);
}

static int sip_uas_onrefer(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session)
{
	return sip_uas_discard(t);
}

// 24 Examples (p213)
void sip_message_test(void)
{
	struct sip_uas_handler_t handler = {
		sip_uas_send,
		sip_uas_onregister,
		sip_uas_oninvite,
		sip_uas_onack,
		sip_uas_onprack,
		sip_uas_onupdate,
		sip_uas_onbye,
		sip_uas_oncancel,
		sip_uas_onsubscribe,
		sip_uas_onnotify,
		sip_uas_onpublish,
		sip_uas_oninfo,
		sip_uas_onmessage,
		sip_uas_onrefer,
	};

	struct sip_message_test_t alice, bob;
	alice.udp = bob.udp = {
		sip_uac_transport_via,
		sip_uac_transport_send,
	};

	alice.sip = sip_agent_create(&handler, &alice);
	bob.sip = sip_agent_create(&handler, &bob);
	assert(0 == sip_test_register(&alice, &bob));
	assert(0 == sip_test_invite(&alice, &bob));
	assert(0 == sip_test_bye(&alice, &bob));

	sip_agent_destroy(alice.sip);
	sip_agent_destroy(bob.sip);
}
