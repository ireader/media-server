#include <memory>
#include <string>
#include <algorithm>
#include "sys/sock.h"
#include "sys/system.h"
#include "sip-uac.h"
#include "sip-uas.h"
#include "sip-header.h"
#include "sip-dialog.h"
#include "sip-message.h"
#include "sip-transport.h"
extern "C" {
#include "../src/uac/sip-uac-transaction.h"
#include "../src/uas/sip-uas-transaction.h"
}
#include "port/ip-route.h"
#include "http-parser.h"
#include "uri-parse.h"
#include "aio-timeout.h"

struct sip_message_test_t
{
	struct sip_agent_t* sip;
	struct sip_transport_t udp;
    struct sip_dialog_t* dialog;
    std::shared_ptr<struct sip_uas_transaction_t> st;
    char buf[1024];
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

static int sip_uac_test_oninvite(void* param, const struct sip_message_t* reply, struct sip_uac_transaction_t* t, struct sip_dialog_t* dialog, int code, void** session)
{
    struct sip_message_test_t* test = (struct sip_message_test_t*)param;
    test->dialog = dialog;
	if (200 <= code && code < 300)
	{
		*session = test;
		sip_uac_ack(t, NULL, 0);
	}
	return 0;
}

static int sip_uac_test_onsubscribe(void* param, const struct sip_message_t* reply, struct sip_uac_transaction_t* t, struct sip_subscribe_t* subscribe, int code, void** session)
{
	return 0;
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
static inline struct sip_uac_transaction_t* sip_uac_transaction_create2(struct sip_message_test_t* test, struct sip_message_t* req)
{
	struct sip_uac_transaction_t* t;
	t = sip_uac_transaction_create(test->sip, req);
	t->oninvite = sip_uac_test_oninvite;
	t->param = test;
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
    struct sip_message_test_t* test = (struct sip_message_test_t*)transport;
    snprintf(test->buf, sizeof(test->buf)-1, "%.*s", (int)bytes, (const char*)data);
	printf("%.*s\n", (int)bytes, (const char*)data);
	return 0;
}

static int sip_uas_send(void* param, const struct cstring_t* /*protocol*/, const struct cstring_t* url, const struct cstring_t* /*received*/, int /*rport*/, const void* data, int bytes)
{
    struct sip_message_test_t* test = (struct sip_message_test_t*)param;
    snprintf(test->buf, sizeof(test->buf)-1, "%.*s", (int)bytes, (const char*)data);
    printf("==> %.*s\n%.*s\n", (int)url->n, url->p, (int)bytes, (const char*)data);
    return 0;
}

static int sip_test_register(struct sip_message_test_t* alice, struct sip_message_test_t *bob)
{
	// F1 REGISTER Bob -> Registrar (p213)
	const char* f1 = "REGISTER sip:registrar.biloxi.com SIP/2.0\r\n"
		"To: Bob <sip:bob@biloxi.com>\r\n"
		"From: Bob <sip:bob@biloxi.com>;tag=456248\r\n"
        "Call-ID: 843817637684230@998sdasdh09\r\n"
		"CSeq: 1826 REGISTER\r\n"
		"Max-Forwards: 70\r\n"
        "Via: SIP/2.0/UDP bobspc.biloxi.com:5060;branch=z9hG4bKnashds7\r\n"
        "Contact: <sip:bob@192.0.2.4>\r\n"
		"Expires: 7200\r\n"
		"Content-Length: 0\r\n\r\n";

	// F2 200 OK Registrar -> Bob (p214)
	const char* f2 = "SIP/2.0 200 OK\r\n"
		"To: Bob <sip:bob@biloxi.com>;tag=2493k59kd\r\n"
		"From: Bob <sip:bob@biloxi.com>;tag=456248\r\n"
		"Call-ID: 843817637684230@998sdasdh09\r\n"
		"CSeq: 1826 REGISTER\r\n"
		"Max-Forwards: 70\r\n"
        "Via: SIP/2.0/UDP bobspc.biloxi.com:5060;branch=z9hG4bKnashds7;received=192.0.2.4\r\n"
        "Contact: <sip:bob@192.0.2.4>\r\n"
		"Expires: 7200\r\n"
		"Content-Length: 0\r\n\r\n";

	struct sip_message_t* req = req2sip(f1);
	struct sip_message_t* reply = reply2sip(f2);
	std::shared_ptr<sip_uac_transaction_t> t(sip_uac_transaction_create1(bob->sip, req), sip_uac_transaction_release);
	assert(0 == sip_uac_send(t.get(), NULL, 0, &bob->udp, bob) && 0 == strcmp(bob->buf, f1));
    assert(0 == sip_agent_set_rport(req, "192.0.2.4", -1));
	assert(0 == sip_agent_input(alice->sip, req, alice));
    sip_message_add_header(alice->st->reply, "To", "Bob <sip:bob@biloxi.com>;tag=2493k59kd"); // modify to.tag
    sip_message_add_header(alice->st->reply, "Contact", "<sip:bob@192.0.2.4>");
    sip_message_add_header(alice->st->reply, "Expires", "7200");
    assert(0 == sip_uas_reply(alice->st.get(), 200, NULL, 0, alice) && 0 == strcmp(alice->buf, f2));
	assert(0 == sip_agent_input(bob->sip, reply, bob));
	//sip_message_destroy(req); // delete by uac transaction
	sip_message_destroy(reply);

	return 0;
}

static int sip_test_invite(struct sip_message_test_t* alice, struct sip_message_test_t *bob)
{
	// F1 INVITE Alice -> atlanta.com proxy (p214)
	const char* f1 = "INVITE sip:bob@biloxi.com SIP/2.0\r\n"
		"To: Bob <sip:bob@biloxi.com>\r\n"
		"From: Alice <sip:alice@atlanta.com>;tag=1928301774\r\n"
		"Call-ID: a84b4c76e66710\r\n"
		"CSeq: 314159 INVITE\r\n"
		"Max-Forwards: 70\r\n"
        "Via: SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bKnashds8\r\n"
        "Contact: <sip:alice@pc33.atlanta.com>\r\n"
		"Content-Type: application/sdp\r\n"
		"Content-Length: 0\r\n\r\n";

	// F2 100 Trying atlanta.com proxy -> Alice (p215)
	const char* f2 = "SIP/2.0 100 Trying\r\n"
		"To: Bob <sip:bob@biloxi.com>;tag=a6c85cf\r\n"
		"From: Alice <sip:alice@atlanta.com>;tag=1928301774\r\n"
		"Call-ID: a84b4c76e66710\r\n"
		"CSeq: 314159 INVITE\r\n"
        "Max-Forwards: 70\r\n"
		"Via: SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bKnashds8;received=192.0.2.1\r\n"
        "Contact: <sip:bob@192.0.2.4>\r\n"
        "Content-Length: 0\r\n\r\n";

	// F8 180 Ringing atlanta.com proxy -> Alice (217)
	const char* f8 = "SIP/2.0 180 Ringing\r\n"
		"To: Bob <sip:bob@biloxi.com>;tag=a6c85cf\r\n"
		"From: Alice <sip:alice@atlanta.com>;tag=1928301774\r\n"
		"Call-ID: a84b4c76e66710\r\n"
		"CSeq: 314159 INVITE\r\n"
        "Max-Forwards: 70\r\n"
        "Via: SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bKnashds8;received=192.0.2.1\r\n"
        "Contact: <sip:bob@192.0.2.4>\r\n"
        "Content-Length: 0\r\n\r\n";

	// F11 200 OK atlanta.com proxy -> Alice (p218)
	const char* f11 = "SIP/2.0 200 OK\r\n"
		"To: Bob <sip:bob@biloxi.com>;tag=a6c85cf\r\n"
		"From: Alice <sip:alice@atlanta.com>;tag=1928301774\r\n"
		"Call-ID: a84b4c76e66710\r\n"
		"CSeq: 314159 INVITE\r\n"
        "Max-Forwards: 70\r\n"
		"Via: SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bKnashds8;received=192.0.2.1\r\n"
        "Contact: <sip:bob@192.0.2.4>\r\n"
		//"Content-Type: application/sdp\r\n"
		"Content-Length: 0\r\n\r\n";

	// F12 ACK Alice -> Bob (p218)
	const char* f12 = "ACK sip:bob@192.0.2.4 SIP/2.0\r\n"
		"To: Bob <sip:bob@biloxi.com>;tag=a6c85cf\r\n"
		"From: Alice <sip:alice@atlanta.com>;tag=1928301774\r\n"
		"Call-ID: a84b4c76e66710\r\n"
		"CSeq: 314159 ACK\r\n"
		"Max-Forwards: 70\r\n"
        "Via: SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bKnashds9\r\n"
        "Content-Length: 0\r\n\r\n";

	const char* f13 = "SIP/2.0 603 Decline\r\n"
		"To: Bob <sip:bob@biloxi.com>;tag=a6c85cf\r\n"
		"From: Alice <sip:alice@atlanta.com>;tag=1928301774\r\n"
		"Call-ID: a84b4c76e66710\r\n"
		"CSeq: 314159 INVITE\r\n"
        "Max-Forwards: 70\r\n"
		"Via: SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bKnashds8;received=192.0.2.1\r\n"
        "Content-Type: application/sdp\r\n"
		"Content-Length: 0\r\n\r\n";

	struct sip_message_t* req = req2sip(f1);
	struct sip_message_t* reply100 = reply2sip(f2);
	struct sip_message_t* reply180 = reply2sip(f8);
	struct sip_message_t* reply200 = reply2sip(f11);
	struct sip_message_t* reply603 = reply2sip(f13);
	struct sip_message_t* ack = req2sip(f12);

	std::shared_ptr<sip_uac_transaction_t> t(sip_uac_transaction_create2(alice, req), sip_uac_transaction_release);
	assert(0 == sip_uac_send(t.get(), NULL, 0, &alice->udp, alice) && 0 == strcmp(alice->buf, f1));
    sip_agent_set_rport(req, "192.0.2.1", -1);
	assert(0 == sip_agent_input(bob->sip, req, bob));
	assert(0 == sip_agent_input(bob->sip, req, bob));
	assert(0 == sip_agent_input(bob->sip, req, bob));
	assert(0 == sip_agent_input(bob->sip, req, bob));
    sip_message_add_header(bob->st->reply, "To", "Bob <sip:bob@biloxi.com>;tag=a6c85cf"); // modify to.tag
    sip_message_add_header(bob->st->reply, "Contact", "<sip:bob@192.0.2.4>");
    assert(0 == sip_uas_reply(bob->st.get(), 100, NULL, 0, bob) && 0 == strcmp(bob->buf, f2));
    assert(0 == sip_agent_input(alice->sip, reply100, alice));
    assert(0 == sip_uas_reply(bob->st.get(), 180, NULL, 0, bob) && 0 == strcmp(bob->buf, f8));
    assert(0 == sip_agent_input(alice->sip, reply180, alice));
    assert(0 == sip_uas_reply(bob->st.get(), 100, NULL, 0, bob) && 0 == strcmp(bob->buf, f2));
    assert(0 == sip_agent_input(alice->sip, reply100, alice));
    assert(0 == sip_uas_reply(bob->st.get(), 180, NULL, 0, bob) && 0 == strcmp(bob->buf, f8));
    assert(0 == sip_agent_input(alice->sip, reply180, alice));
    //assert(0 == sip_uas_reply(bob->st, 603, NULL, 0));
    //assert(0 == sip_agent_input(alice->sip, reply603));
    assert(0 == sip_uas_reply(bob->st.get(), 200, NULL, 0, bob) && 0 == strcmp(bob->buf, f11));
    assert(0 == sip_agent_input(alice->sip, reply200, alice) /*&& 0 == strcmp(alice->buf, f12)*/ ); // The branch parameter is different for  INVITE and ACK message sent By UAC
	assert(0 == sip_agent_input(alice->sip, reply180, alice));
	assert(0 == sip_agent_input(alice->sip, reply100, alice));
	assert(0 == sip_agent_input(alice->sip, reply200, alice));
	assert(0 == sip_agent_input(bob->sip, ack, bob));
	assert(0 == sip_agent_input(bob->sip, ack, bob));
	assert(0 == sip_agent_input(bob->sip, ack, bob));

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
	std::shared_ptr<sip_uac_transaction_t> t(sip_uac_transaction_create1(bob->sip, req), sip_uac_transaction_release);
	assert(0 == sip_uac_send(t.get(), "Watson, come here.", 18, &bob->udp, bob));
	assert(0 == sip_agent_input(alice->sip, req, alice));
    assert(0 == sip_uas_reply(alice->st.get(), 200, NULL, 0, alice));
	assert(0 == sip_agent_input(bob->sip, reply, bob));
	//sip_message_destroy(req); // delete by uac transaction
	sip_message_destroy(reply);

	return 0;
}

static int sip_test_dialog_message(struct sip_message_test_t* alice, struct sip_message_test_t* bob)
{
    // https://tools.ietf.org/html/rfc3428#page-11
    // F1 MESSAGE Bob -> Alice
    const char* f1 = "MESSAGE sip:alice@atlanta.com SIP/2.0\n"
        "Via: SIP/2.0/TCP biloxi.com;branch=z9hG4bK776sgdkse2\n"
        "Max-Forwards: 70\n"
        "From: Bob <sip:bob@biloxi.com>;tag=a6c85cf\r\n"
        "To: Alice <sip:alice@atlanta.com>;tag=1928301774\r\n"
        "Call-ID: a84b4c76e66710\r\n"
        "CSeq: 1 MESSAGE\n"
        "Content-Type: text/plain\n\n";
        //"Content-Length: 18\n"
        //"\r\n"
        //"Watson, come here.";

    // F2 200 OK Alice -> Bob
    const char* f2 = "SIP/2.0 200 OK\r\n"
        "Via: SIP/2.0/TCP biloxi.com;branch=z9hG4bK776sgdkse; received=1.2.3.4\r\n"
        "From: Bob <sip:bob@biloxi.com>;tag=a6c85cf\r\n"
        "To: Alice <sip:alice@atlanta.com>;tag=1928301774\r\n"
        "Call-ID: a84b4c76e66710\r\n"
        "CSeq: 1 MESSAGE\r\n"
        "Content-Length: 0\r\n\r\n";

    struct sip_message_t* req = req2sip(f1);
    struct sip_message_t* reply = reply2sip(f2);
    std::shared_ptr<sip_uac_transaction_t> t(sip_uac_transaction_create1(bob->sip, req), sip_uac_transaction_release);
    assert(0 == sip_uac_send(t.get(), "Watson, come here.", 18, &bob->udp, bob));
    assert(0 == sip_agent_input(alice->sip, req, alice));
    assert(0 == sip_uas_reply(alice->st.get(), 200, NULL, 0, alice));
    assert(0 == sip_agent_input(bob->sip, reply, bob));
    //sip_message_destroy(req); // delete by uac transaction
    sip_message_destroy(reply);

    return 0;
}

static int sip_test_notify(struct sip_message_test_t* alice, struct sip_message_test_t* bob)
{
	// https://tools.ietf.org/html/rfc3428#page-11
	// F1 SUBSCRIBE Alice -> Bob
	const char* f1 = "SUBSCRIBE sip:bob@biloxi.com;user=phone SIP/2.0\n"
		"To: Bob <sip:bob@biloxi.com>;tag=a6c85cf\n"
		"From: Alice <sip:alice@atlanta.com>;tag=1928301774\n"
		"Call-ID: a84b4c76e66710\n"
		"CSeq: 314160 SUBSCRIBE\n"
		"Max-Forwards: 70\n"
        "Via: SIP/2.0/UDP 10.135.0.12:5060;branch=z9hG4bKtrxftxslfcy3aagf3c9s7\n"
        "Contact: <sip:150@10.135.0.12;line=16172>;+sip.instance=\"<urn:uuid:0d9a008d-0355-0024-0000-000276f3d796>\"\n"
		"Event: message-summary\n"
        "Allow-Events: dialog,message-summary\n"
        "Accept: application/simple-message-summary\n"
		"Allow: INVITE, CANCEL, BYE, ACK, REGISTER, OPTIONS, REFER, SUBSCRIBE, NOTIFY, MESSAGE, INFO, PRACK, UPDATE\n"
		"Expires: 240\n"
		"Supported: replaces,100rel\n"
		"User-Agent: Wildix W-AIR 03.55.00.24 9c7514340722 02:76:f3:d7:96\n"
		"Content-Length: 0\n\n";

	const char* f2 = "SIP/2.0 200 OK\n"
		"To: Bob <sip:bob@biloxi.com>;tag=a6c85cf\n"
		"From: Alice <sip:alice@atlanta.com>;tag=1928301774\n"
		"Call-ID: a84b4c76e66710\n"
		"CSeq: 314160 SUBSCRIBE\n"
		"Max-Forwards: 70\n"
        //"Via: SIP/2.0/UDP 10.135.0.12:5060;branch=z9hG4bKtrxftxslfcy3aagf3c9s7;rport=5060\n"
        "Via: SIP/2.0/UDP 10.135.0.12:5060;branch=z9hG4bKtrxftxslfcy3aagf3c9s7\n"
        "Contact: <sip:vmaccess*150@mypbx.wildixin.com:5060;user=phone>\n"
		"Expires: 240\n"
        "Server: Wildix GW-4.2.5.35963\n"
		"Content-Length: 0\n\n";

	const char* f3 = "NOTIFY sip:alice@atlanta.com;line=16172 SIP/2.0\n"
		"To: Alice <sip:alice@atlanta.com>;tag=1928301774\n"
        "From: Bob <sip:bob@biloxi.com>;tag=a6c85cf\n"
		"Call-ID: a84b4c76e66710\n"
		"CSeq: 2 NOTIFY\n"
		"Max-Forwards: 70\n"
		"Via: SIP/2.0/UDP 10.135.0.1;branch=z9hG4bK1308.a003bf56000000000000000000000000.0\n"
        "Contact: <sip:vmaccess*150@mypbx.wildixin.com:5060;user=phone>\n"
        "Event: message-summary\n"
		"Subscription-State: active;expires=240\n"
		"User-Agent: Wildix GW-4.2.5.35963\n"
        "Content-Type: application/simple-message-summary\n"
		//"Content-Length: 90\n"
		"\n";
	const char* f3message = 
		"Messages-Waiting: yes\n"
		"Message-Account: sip:vmaccess*150@wildix\n"
		"Voice-Message: 1/0 (1/0)";

	const char* f4 = "SIP/2.0 200 OK\n"
		"To: Alice <sip:alice@atlanta.com>;tag=1928301774\n"
		"From: Bob <sip:bob@biloxi.com>;tag=a6c85cf\n"
        "Call-ID: a84b4c76e66710\n"
		"CSeq: 2 NOTIFY\n"
		"Max-Forwards: 70\n"
        "Via: SIP/2.0/UDP 10.135.0.1;branch=z9hG4bK1308.a003bf56000000000000000000000000.0\n"
        "Event: message-summary\n"
		"Subscription-State: active;expires=240\n"
		"User-Agent: Wildix W-AIR 03.55.00.24 9c7514340722\n"
		"Content-Length: 0\n\n";

    const char* f5 = "SUBSCRIBE sip:bob@biloxi.com;user=phone SIP/2.0\n"
        "To: Bob <sip:bob@biloxi.com>;tag=a6c85cf\n"
        "From: Alice <sip:alice@atlanta.com>;tag=1928301774\n"
        "Call-ID: a84b4c76e66710\n"
        "CSeq: 314161 SUBSCRIBE\n"
        "Max-Forwards: 70\n"
        "Via: SIP/2.0/UDP 10.135.0.12:5060;branch=z9hG4bKtrxftxslfcy3aagf3c9s7.1\n"
        "Contact: <sip:150@10.135.0.12;line=16172>;+sip.instance=\"<urn:uuid:0d9a008d-0355-0024-0000-000276f3d796>\"\n"
        "Event: message-summary\n"
        "Allow-Events: dialog,message-summary\n"
        "Accept: application/simple-message-summary\n"
        "Allow: INVITE, CANCEL, BYE, ACK, REGISTER, OPTIONS, REFER, SUBSCRIBE, NOTIFY, MESSAGE, INFO, PRACK, UPDATE\n"
        "Expires: 0\n"
        "Supported: replaces,100rel\n"
        "User-Agent: Wildix W-AIR 03.55.00.24 9c7514340722 02:76:f3:d7:96\n"
        "Content-Length: 0\n\n";

    const char* f6 = "SIP/2.0 200 OK\n"
        "To: Bob <sip:bob@biloxi.com>;tag=a6c85cf\n"
        "From: Alice <sip:alice@atlanta.com>;tag=1928301774\n"
        "Call-ID: a84b4c76e66710\n"
        "CSeq: 314161 SUBSCRIBE\n"
        "Max-Forwards: 70\n"
        //"Via: SIP/2.0/UDP 10.135.0.12:5060;branch=z9hG4bKtrxftxslfcy3aagf3c9s7.1;rport=5060\n"
        "Via: SIP/2.0/UDP 10.135.0.12:5060;branch=z9hG4bKtrxftxslfcy3aagf3c9s7.1\n"
        "Contact: <sip:vmaccess*150@mypbx.wildixin.com:5060;user=phone>\n"
        "Expires: 0\n"
        "Server: Wildix GW-4.2.5.35963\n"
        "Content-Length: 0\n\n";
    
	struct sip_message_t* req = req2sip(f1);
	struct sip_message_t* reply = reply2sip(f2);
	std::shared_ptr<sip_uac_transaction_t> t(sip_uac_transaction_create3(alice->sip, req), sip_uac_transaction_release);
    assert(0 == sip_uac_send(t.get(), NULL, 0, &alice->udp, alice));
    std::string s(alice->buf); memset(alice->buf, 0, sizeof(alice->buf)); std::copy_if(s.begin(), s.end(), alice->buf, [](char c){return c!='\r';});
    assert(0 == strcmp(alice->buf, f1));
	assert(0 == sip_agent_input(bob->sip, req, bob));
    sip_message_add_header(bob->st->reply, "Expires", "240");
    sip_message_add_header(bob->st->reply, "Contact", "<sip:vmaccess*150@mypbx.wildixin.com:5060;user=phone>");
    sip_message_add_header(bob->st->reply, "Server", "Wildix GW-4.2.5.35963");
    assert(0 == sip_uas_reply(bob->st.get(), 200, NULL, 0, bob));
	std::string s2(bob->buf); memset(bob->buf, 0, sizeof(bob->buf)); std::copy_if(s2.begin(), s2.end(), bob->buf, [](char c){return c!='\r';});
    assert(0 == strcmp(bob->buf, f2));
    assert(0 == sip_agent_input(alice->sip, reply, alice));
    
    struct sip_message_t* req2 = req2sip(f3);
    struct sip_message_t* reply2 = reply2sip(f4);
	std::shared_ptr<sip_uac_transaction_t> t2(sip_uac_transaction_create1(bob->sip, req2), sip_uac_transaction_release);
	assert(0 == sip_uac_send(t2.get(), f3message, strlen(f3message), &bob->udp, bob));
    std::string s3(bob->buf); memset(bob->buf, 0, sizeof(bob->buf)); std::copy_if(s3.begin(), s3.end(), bob->buf, [](char c){return c!='\r';});
    assert(0 == strncmp(bob->buf, f3, strlen(f3)-1)); // miss content-length
	assert(0 == sip_agent_input(alice->sip, req2, alice));
    sip_message_add_header(alice->st->reply, "Event", "message-summary");
    sip_message_add_header(alice->st->reply, "Subscription-State", "active;expires=240");
    sip_message_add_header(alice->st->reply, "User-Agent", "Wildix W-AIR 03.55.00.24 9c7514340722");
    assert(0 == sip_uas_reply(alice->st.get(), 200, NULL, 0, alice));
    std::string s4(alice->buf); memset(alice->buf, 0, sizeof(alice->buf)); std::copy_if(s4.begin(), s4.end(), alice->buf, [](char c){return c!='\r';});
    assert(0 == strcmp(alice->buf, f4));
	assert(0 == sip_agent_input(bob->sip, reply2, bob));
    
    struct sip_message_t* req3 = req2sip(f5);
    struct sip_message_t* reply3 = reply2sip(f6);
    std::shared_ptr<sip_uac_transaction_t> t3(sip_uac_transaction_create3(alice->sip, req3), sip_uac_transaction_release);
    assert(0 == sip_uac_send(t3.get(), NULL, 0, &alice->udp, alice));
    std::string s5(alice->buf); memset(alice->buf, 0, sizeof(alice->buf)); std::copy_if(s5.begin(), s5.end(), alice->buf, [](char c){return c!='\r';});
    assert(0 == strcmp(alice->buf, f5));
    assert(0 == sip_agent_input(bob->sip, req3, bob));
    sip_message_add_header(bob->st->reply, "Expires", "0");
    sip_message_add_header(bob->st->reply, "Contact", "<sip:vmaccess*150@mypbx.wildixin.com:5060;user=phone>");
    sip_message_add_header(bob->st->reply, "Server", "Wildix GW-4.2.5.35963");
    assert(0 == sip_uas_reply(bob->st.get(), 200, NULL, 0, bob));
    std::string s6(bob->buf); memset(bob->buf, 0, sizeof(bob->buf)); std::copy_if(s6.begin(), s6.end(), bob->buf, [](char c){return c!='\r';});
    assert(0 == strcmp(bob->buf, f6));
    assert(0 == sip_agent_input(alice->sip, reply3, alice));
    
	//sip_message_destroy(req); // delete by uac transaction
    //sip_message_destroy(req2); // delete by uac transaction
    //sip_message_destroy(req3); // delete by uac transaction
	sip_message_destroy(reply);
	sip_message_destroy(reply2);
    sip_message_destroy(reply3);

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
		"CSeq: 314160 BYE\r\n"
		"Content-Length: 0\r\n\r\n";

	// F14 200 OK Alice -> Bob (p219)
	const char* f14 = "SIP/2.0 200 OK\r\n"
		"Via: SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bKnashds9\r\n"
		"To: Bob <sip:bob@biloxi.com>;tag=a6c85cf\r\n"
		"From: Alice <sip:alice@atlanta.com>;tag=1928301774\r\n"
		"Call-ID: a84b4c76e66710\r\n"
		"CSeq: 314160 BYE\r\n"
		"Content-Length: 0\r\n\r\n";

	struct sip_message_t* req = req2sip(f13);
	struct sip_message_t* reply = reply2sip(f14);
	std::shared_ptr<sip_uac_transaction_t> t(sip_uac_transaction_create1(alice->sip, req), sip_uac_transaction_release);
    //std::shared_ptr<sip_uac_transaction_t> t(sip_uac_bye(alice->sip, alice->dialog, sip_uac_test_onreply, NULL), sip_uac_transaction_release);
    //sip_uac_add_header(t.get(), "Via", "SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bKnashds9");
    assert(0 == sip_uac_send(t.get(), NULL, 0, &alice->udp, alice));
	assert(0 == sip_agent_input(bob->sip, req, bob));
    assert(0 == sip_uas_reply(bob->st.get(), 200, NULL, 0, bob));
	assert(0 == sip_agent_input(alice->sip, reply, alice));
	//sip_message_destroy(req); // delete by uac transaction
	sip_message_destroy(reply);

	return 0;
}

static int sip_uas_oninvite(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, struct sip_dialog_t* dialog, const void* data, int bytes, void** session)
{
    struct sip_message_test_t* test = (struct sip_message_test_t*)param;
    test->dialog = dialog;
    
    sip_uas_transaction_addref(t);
    test->st.reset(t, sip_uas_transaction_release);
	*session = test;
    return 0;
}

static int sip_uas_onack(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session, struct sip_dialog_t* dialog, int code, const void* data, int bytes)
{
    struct sip_message_test_t* test = (struct sip_message_test_t*)param;
    sip_uas_transaction_addref(t);
    test->st.reset(t, sip_uas_transaction_release);
    return 0;
}

static int sip_uas_onbye(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session)
{
	struct sip_message_test_t* test = (struct sip_message_test_t*)param;
    sip_uas_transaction_addref(t);
    test->st.reset(t, sip_uas_transaction_release);
    return 0;
}

static int sip_uas_oncancel(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session)
{
	struct sip_message_test_t* test = (struct sip_message_test_t*)param;
    sip_uas_transaction_addref(t);
    test->st.reset(t, sip_uas_transaction_release);
    return 0;
}

static int sip_uas_onprack(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session, struct sip_dialog_t* dialog, const void* data, int bytes)
{
	struct sip_message_test_t* test = (struct sip_message_test_t*)param;
    sip_uas_transaction_addref(t);
    test->st.reset(t, sip_uas_transaction_release);
    return 0;
}

static int sip_uas_onupdate(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session, struct sip_dialog_t* dialog, const void* data, int bytes)
{
	struct sip_message_test_t* test = (struct sip_message_test_t*)param;
    sip_uas_transaction_addref(t);
    test->st.reset(t, sip_uas_transaction_release);
    return 0;
}

static int sip_uas_oninfo(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session, struct sip_dialog_t* dialog, const struct cstring_t* package, const void* data, int bytes)
{
    struct sip_message_test_t* test = (struct sip_message_test_t*)param;
    sip_uas_transaction_addref(t);
    test->st.reset(t, sip_uas_transaction_release);
    return 0;
}

static int sip_uas_onregister(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, const char* user, const char* location, int expires)
{
	assert(expires == 7200);
	assert(0 == strcmp(user, "bob"));
	assert(0 == strcmp(location, "192.0.2.4"));
	struct sip_message_test_t* test = (struct sip_message_test_t*)param;
    sip_uas_transaction_addref(t);
    test->st.reset(t, sip_uas_transaction_release);
    return 0;
}

static int sip_uas_onmessage(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session, const void* payload, int bytes)
{
	struct sip_message_test_t* test = (struct sip_message_test_t*)param;
    sip_uas_transaction_addref(t);
    test->st.reset(t, sip_uas_transaction_release);
    return 0;
}

static int sip_uas_onsubscribe(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, sip_subscribe_t* subscribe, void** sub)
{
	struct sip_message_test_t* test = (struct sip_message_test_t*)param;
    sip_uas_transaction_addref(t);
    test->st.reset(t, sip_uas_transaction_release);

	//assert(0 == sip_uas_reply(t, 200, NULL, 0));
	//std::shared_ptr<sip_uac_transaction_t> notify(sip_uac_notify(test->sip, subscribe, "active", sip_uac_test_onreply, test), sip_uac_transaction_release);
	//assert(0 == sip_uac_send(notify.get(), NULL, 0, &test->udp, test));
	
	*sub = test;
	return 0;
}

static int sip_uas_onnotify(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session, const struct cstring_t* event)
{
	struct sip_message_test_t* test = (struct sip_message_test_t*)param;
    sip_uas_transaction_addref(t);
    test->st.reset(t, sip_uas_transaction_release);
    return 0;
}

static int sip_uas_onpublish(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, const struct cstring_t* event)
{
	struct sip_message_test_t* test = (struct sip_message_test_t*)param;
    sip_uas_transaction_addref(t);
    test->st.reset(t, sip_uas_transaction_release);
    return 0;
}

static int sip_uas_onrefer(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session)
{
	struct sip_message_test_t* test = (struct sip_message_test_t*)param;
    sip_uas_transaction_addref(t);
    test->st.reset(t, sip_uas_transaction_release);
    return 0;
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
		sip_uas_oninfo,
        sip_uas_onbye,
		sip_uas_oncancel,
		sip_uas_onsubscribe,
		sip_uas_onnotify,
		sip_uas_onpublish,
		sip_uas_onmessage,
		sip_uas_onrefer,
	};

	struct sip_message_test_t alice, bob;
	alice.udp = bob.udp = {
		sip_uac_transport_via,
		sip_uac_transport_send,
	};

	alice.sip = sip_agent_create(&handler);
	bob.sip = sip_agent_create(&handler);
	assert(0 == sip_test_register(&alice, &bob));
	assert(0 == sip_test_message(&alice, &bob));
	assert(0 == sip_test_invite(&alice, &bob));
    assert(0 == sip_test_dialog_message(&alice, &bob));
    assert(0 == sip_test_notify(&alice, &bob));
	assert(0 == sip_test_bye(&alice, &bob));

	sip_agent_destroy(alice.sip);
	sip_agent_destroy(bob.sip);
}
