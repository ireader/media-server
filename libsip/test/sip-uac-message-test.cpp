#include "sys/sock.h"
#include "sip-uac.h"
#include "sip-message.h"
#include "sip-transport.h"
#include "port/ip-route.h"
#include "http-parser.h"
#include "uri-parse.h"

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
			if(0 == socket_addr_from(&ss, &len, local, 0))
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
	//char p1[1024];
	//char p2[1024];
	((char*)data)[bytes] = 0;
	printf("%s\n", (const char*)data);
	struct sip_message_t* msg = req2sip((const char*)data);
	struct sip_message_t* req = (struct sip_message_t*)transport;
	assert(msg->mode == req->mode && (cstreq(&msg->u.c.method, &req->u.c.method) || 0 == cstrcasecmp(&msg->u.c.method, "ACK")));
//	assert(sip_contact_write(&msg->to, p1, p1 + sizeof(p1)) > 0 && sip_contact_write(&req->to, p2, p2 + sizeof(p2)) > 0 && 0 == strcmp(p1, p2));
//	assert(sip_contact_write(&msg->from, p1, p1 + sizeof(p1)) > 0 && sip_contact_write(&req->from, p2, p2 + sizeof(p2)) > 0 && 0 == strcmp(p1, p2));
	return 0;
}

static void* sip_uac_message_oninvite(void* param, const struct sip_message_t* reply, struct sip_uac_transaction_t* t, struct sip_dialog_t* dialog, int code)
{
	s_dialog = dialog;
	return NULL;
}

static int sip_uac_message_onregister(void* param, const struct sip_message_t* reply, struct sip_uac_transaction_t* t, int code)
{
	return 0;
}

static int sip_uac_message_onbye(void* param, const struct sip_message_t* reply, struct sip_uac_transaction_t* t, int code)
{
	return 0;
}

static void sip_uac_message_register(struct sip_agent_t* sip, struct sip_transport_t* udp)
{
	// F1 REGISTER Bob -> Registrar (p213)
	const char* f1 = "REGISTER sip:registrar.biloxi.com SIP/2.0\r\n"
		"Via: SIP/2.0/UDP bobspc.biloxi.com:5060;branch=z9hG4bKnashds7\r\n"
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

	struct sip_uac_transaction_t* t;
	//t = sip_uac_register(sip, "Bob <sip:bob@biloxi.com>", "sip:registrar.biloxi.com", 7200, sip_uac_message_onregister, NULL);
	t = sip_uac_register(sip, "Bob <sip:bob@biloxi.com>", NULL, 7200, sip_uac_message_onregister, NULL);
	sip_uac_add_header(t, "Via", "SIP/2.0/UDP bobspc.biloxi.com:5060;branch=z9hG4bKnashds7");
	sip_uac_add_header(t, "CSeq", "1826 REGISTER");// modify cseq.id
	assert(0 == sip_uac_send(t, NULL, 0, udp, req));
	assert(0 == sip_agent_input(sip, reply));

	sip_message_destroy(req);
	sip_message_destroy(reply);
	//sip_uac_transaction_release(t);
}

static void sip_uac_message_invite(struct sip_agent_t* sip, struct sip_transport_t* udp)
{
	// F1 INVITE Alice -> atlanta.com proxy (p214)
	const char* f1 = "INVITE sip:bob@biloxi.com SIP/2.0\r\n"
		"Via: SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bKnashds8\r\n"
		"Max-Forwards: 70\r\n"
		"To: Bob <sip:bob@biloxi.com>\r\n"
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

	struct sip_uac_transaction_t* t = sip_uac_invite(sip, "Alice <sip:alice@atlanta.com>;tag=1928301774", "Bob <sip:bob@biloxi.com>", sip_uac_message_oninvite, NULL);
	sip_uac_add_header(t, "Via", "SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bKnashds8");
	sip_uac_add_header(t, "Call-ID", "a84b4c76e66710");// modify call-id
	sip_uac_add_header(t, "CSeq", "314159 INVITE");// modify cseq.id
	sip_uac_add_header(t, "Content-Type", "application/sdp");
	sip_uac_add_header_int(t, "Content-Length", 3);
	sip_uac_send(t, "sdp", 3, udp, req);

	sip_agent_input(sip, reply100);
	sip_agent_input(sip, reply180);
	sip_agent_input(sip, reply100);
	sip_agent_input(sip, reply180);
//	sip_agent_input(sip, reply603);
	sip_agent_input(sip, reply200);
	sip_agent_input(sip, reply180);
	sip_agent_input(sip, reply100);
	sip_agent_input(sip, reply200);

	sip_message_destroy(req);
	sip_message_destroy(reply100);
	sip_message_destroy(reply180);
	sip_message_destroy(reply200);
	sip_message_destroy(ack);
	//sip_uac_transaction_release(t);
}

static void sip_uac_message_bye(struct sip_agent_t* sip, struct sip_transport_t* udp)
{
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

	struct sip_uac_transaction_t* t = sip_uac_bye(sip, s_dialog, sip_uac_message_onbye, NULL);
	sip_uac_add_header(t, "Via", "SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bKnashds9");
	sip_uac_add_header_int(t, "Content-Length", 0);
	sip_uac_send(t, NULL, 0, udp, req);
	sip_agent_input(sip, reply);

	sip_message_destroy(req);
	sip_message_destroy(reply);
	//sip_uac_transaction_release(t);
}

// 24 Examples (p213)
void sip_uac_message_test(void)
{
	struct sip_transport_t udp = {
		sip_uac_transport_via,
		sip_uac_transport_send,
	};
	struct sip_uas_handler_t handler;
	memset(&handler, 0, sizeof(handler));
	struct sip_agent_t* sip = sip_agent_create(&handler, NULL);
	sip_uac_message_register(sip, &udp);
	sip_uac_message_invite(sip, &udp);
	sip_uac_message_bye(sip, &udp);
	sip_agent_destroy(sip);
}
