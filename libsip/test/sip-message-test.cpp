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
	r = socket_addr_from(&ss, &len, "127.0.0.1" /*uri->host*/, uri->port ? uri->port : SIP_PORT);
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

static int sip_uas_send(void* param, const struct cstring_t* /*protocol*/, const struct cstring_t* url, const struct cstring_t* /*received*/, int /*rport*/, const void* data, int bytes)
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

static int sip_test_message(struct sip_message_test_t* alice, struct sip_message_test_t* bob)
{
	// https://tools.ietf.org/html/rfc3428#page-11
	// F1 MESSAGE Bob -> Alice
	const char* f1 = "MESSAGE sip:alice@atlanta.com SIP/2.0\n"
		"Via: SIP/2.0/TCP biloxi.com;branch=z9hG4bK776sgdkse\n"
		"Max-Forwards: 70\n"
		"From: Bob <sip:bob@biloxi.com>;tag=a6c85cf\r\n"
		"To: Alice <sip:alice@atlanta.com>;tag=1928301774\r\n"
		"Call-ID: asd88asd77a@1.2.3.4\n"
		"CSeq: 1 MESSAGE\n"
		"Content-Type: text/plain\n\n";
		//"Content-Length: 18\n"
		//"\r\n"
		//"Watson, come here.";

	const char* f2 = "MESSAGE sip:user2@domain.com SIP/2.0\r\n"
		"Via: SIP/2.0/TCP proxy.domain.com;branch=z9hG4bK123dsghds\r\n"
		"Via: SIP/2.0/TCP user1pc.domain.com;branch=z9hG4bK776sgdkse; received=1.2.3.4\r\n"
		"Max-Forwards: 69\r\n"
		"From: sip:user1@domain.com;tag=49394\r\n"
		"To: sip:user2@domain.com\r\n"
		"Call-ID: asd88asd77a@1.2.3.4\r\n"
		"CSeq: 1 MESSAGE\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: 18\r\n"
		"\r\n"
		"Watson, come here.";

	const char* f3 = "SIP/2.0 200 OK\r\n"
		"Via: SIP/2.0/TCP proxy.domain.com;branch=z9hG4bK123dsghds; received=192.0.2.1\r\n"
		"Via: SIP/2.0/TCP user1pc.domain.com;branch=z9hG4bK776sgdkse; received=1.2.3.4\r\n"
		"From: sip:user1@domain.com;tag=49394\r\n"
		"To: sip:user2@domain.com;tag=ab8asdasd9\r\n"
		"Call-ID: asd88asd77a@1.2.3.4\r\n"
		"CSeq: 1 MESSAGE\r\n"
		"Content-Length: 0\r\n\r\n";

	// F2 200 OK Alice -> Bob
	const char* f4 = "SIP/2.0 200 OK\r\n"
		"Via: SIP/2.0/TCP biloxi.com;branch=z9hG4bK776sgdkse; received=1.2.3.4\r\n"
		"From: Bob <sip:bob@biloxi.com>;tag=a6c85cf\r\n"
		"To: Alice <sip:alice@atlanta.com>;tag=1928301774\r\n"
		"Call-ID: asd88asd77a@1.2.3.4\r\n"
		"CSeq: 1 MESSAGE\r\n"
		"Content-Length: 0\r\n\r\n";

	struct sip_message_t* req = req2sip(f1);
	struct sip_message_t* reply = reply2sip(f4);
	assert(0 == sip_uac_send(sip_uac_transaction_create1(bob->sip, req), "Watson, come here.", 18, &bob->udp, &bob));
	assert(0 == sip_agent_input(alice->sip, req));
	assert(0 == sip_agent_input(bob->sip, reply));
	//sip_message_destroy(req); // delete by uac transaction
	sip_message_destroy(reply);

	return 0;
}

static int sip_test_notify(struct sip_message_test_t* alice, struct sip_message_test_t* bob)
{
	// https://tools.ietf.org/html/rfc3428#page-11
	// F1 SUBSCRIBE Alice -> Bob
	const char* f1 = "SUBSCRIBE sip:bob@biloxi.com;user=phone SIP/2.0\n"
		"Via: SIP/2.0/UDP 10.135.0.12:5060;branch=z9hG4bKtrxftxslfcy3aagf3c9s7\n"
		"Max-Forwards: 70\n"
		"To: Bob <sip:bob@biloxi.com>;tag=a6c85cf\r\n"
		"From: Alice <sip:alice@atlanta.com>;tag=1928301774\r\n"
		"Call-ID: a84b4c76e66710\r\n"
		"CSeq: 314160 SUBSCRIBE\r\n"
		"Contact: <sip:150@10.135.0.12;line=16172>;+sip.instance=\"<urn:uuid:0d9a008d-0355-0024-0000-000276f3d796>\"\n"
		"Accept: application/simple-message-summary\n"
		"Allow: INVITE, CANCEL, BYE, ACK, REGISTER, OPTIONS, REFER, SUBSCRIBE, NOTIFY, MESSAGE, INFO, PRACK, UPDATE\n"
		"Allow-Events: dialog,message-summary\n"
		"Event: message-summary\n"
		"Expires: 240\n"
		"Supported: replaces,100rel\n"
		"User-Agent: Wildix W-AIR 03.55.00.24 9c7514340722 02:76:f3:d7:96\n"
		"Content-Length: 0\n\n";

	const char* f2 = "SIP/2.0 200 OK\n"
		"Via: SIP/2.0/UDP 10.135.0.12:5060;branch=z9hG4bKtrxftxslfcy3aagf3c9s7;rport=5060\n"
		"To: Bob <sip:bob@biloxi.com>;tag=a6c85cf\r\n"
		"From: Alice <sip:alice@atlanta.com>;tag=1928301774\r\n"
		"Call-ID: a84b4c76e66710\r\n"
		"CSeq: 314160 SUBSCRIBE\r\n"
		"Expires: 240\n"
		"Contact: <sip:vmaccess*150@mypbx.wildixin.com:5060;user=phone> \n"
		"Server: Wildix GW-4.2.5.35963\n"
		"Content-Length: 0\r\n\r\n";

	const char* f3 = "NOTIFY sip:alice@atlanta.com;line=16172 SIP/2.0\n"
		"Via: SIP/2.0/UDP 10.135.0.1;branch=z9hG4bK1308.a003bf56000000000000000000000000.0\n"
		"From: Bob <sip:bob@biloxi.com>;tag=a6c85cf\r\n"
		"To: Alice <sip:alice@atlanta.com>;tag=1928301774\r\n"
		"Call-ID: a84b4c76e66710\r\n"
		"CSeq: 314161 NOTIFY\r\n"
		"User-Agent: Wildix GW-4.2.5.35963\n"
		"Max-Forwards: 70\n"
		"Event: message-summary\n"
		"Contact: <sip:vmaccess*150@mypbx.wildixin.com:5060;user=phone> \n"
		"Subscription-State: active;expires=240\n"
		"Content-Type: application/simple-message-summary\n"
		//"Content-Length: 90\n"
		"\n";
	const char* f3message = 
		"Messages-Waiting: yes\n"
		"Message-Account: sip:vmaccess*150@wildix\n"
		"Voice-Message: 1/0 (1/0)";

	const char* f4 = "SIP/2.0 200 OK\n"
		"Via: SIP/2.0/UDP 10.135.0.1;branch=z9hG4bK1308.a003bf56000000000000000000000000.0\n"
		"Max-Forwards: 70\n"
		"From: Bob <sip:bob@biloxi.com>;tag=a6c85cf\r\n"
		"To: Alice <sip:alice@atlanta.com>;tag=1928301774\r\n"
		"Call-ID: a84b4c76e66710\r\n"
		"CSeq: 314161 NOTIFY\r\n"
		"Event: message-summary\n"
		"Subscription-State: active;expires=240\n"
		"User-Agent: Wildix W-AIR 03.55.00.24 9c7514340722\n"
		"Content-Length: 0\r\n\r\n";

	struct sip_message_t* req = req2sip(f1);
	struct sip_message_t* reply = reply2sip(f2); 
	struct sip_message_t* req2 = req2sip(f3);
	struct sip_message_t* reply2 = reply2sip(f4);
	assert(0 == sip_uac_send(sip_uac_transaction_create3(alice->sip, req), NULL, 0, &alice->udp, &alice));
	assert(0 == sip_agent_input(bob->sip, req));
	assert(0 == sip_agent_input(alice->sip, reply));
	assert(0 == sip_uac_send(sip_uac_transaction_create1(bob->sip, req2), f3message, strlen(f3message), &bob->udp, &bob));
	assert(0 == sip_agent_input(alice->sip, req2));
	assert(0 == sip_agent_input(bob->sip, reply2));
	//sip_message_destroy(req); // delete by uac transaction
	sip_message_destroy(reply);
	sip_message_destroy(reply2);

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
	assert(0 == sip_test_message(&alice, &bob));
	assert(0 == sip_test_invite(&alice, &bob));
	assert(0 == sip_test_notify(&alice, &bob));
	assert(0 == sip_test_bye(&alice, &bob));

	sip_agent_destroy(alice.sip);
	sip_agent_destroy(bob.sip);
}
