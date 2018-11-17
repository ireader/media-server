#include "sockutil.h"
#include "aio-timeout.h"
#include "sip-uac.h"
#include "sip-message.h"
#include "sip-transport.h"
#include "port/ip-route.h"
#include "http-parser.h"
#include "http-header-auth.h"
#include "sys/thread.h"
#include "sys/system.h"
#include "uri-parse.h"
#include "cstringext.h"
#include "base64.h"
#include "md5.h"
#include <stdint.h>
#include <string.h>
#include <ctype.h>

struct sip_uac_test_t
{
	socket_t udp;
	socklen_t addrlen;
	struct sockaddr_storage addr;

	http_parser_t* parser;
	struct sip_uac_t* uac;
	struct sip_transport_t transport;
};

static int sip_uac_transport_via(void* transport, const char* destination, char protocol[16], char local[128], char dns[128])
{
	int r;
	char ip[65];
	u_short port;
	struct uri_t* uri;
	
	struct sip_uac_test_t *test = (struct sip_uac_test_t *)transport;

	// rfc3263 4.1 Selecting a Transport Protocol
	// 1. If the URI specifies a transport protocol in the transport parameter,
	//    that transport protocol SHOULD be used. Otherwise, if no transport 
	//    protocol is specified, but the TARGET(maddr) is a numeric IP address, 
	//    the client SHOULD use UDP for a SIP URI, and TCP for a SIPS URI.
	// 2. if no transport protocol is specified, and the TARGET is not numeric, 
	//    but an explicit port is provided, the client SHOULD use UDP for a SIP URI, 
	//    and TCP for a SIPS URI
	// 3. Otherwise, if no transport protocol or port is specified, and the target 
	//    is not a numeric IP address, the client SHOULD perform a NAPTR query for 
	//    the domain in the URI.

	// The client SHOULD try the first record. If an attempt should fail, based on 
	// the definition of failure in Section 4.3, the next SHOULD be tried, and if 
	// that should fail, the next SHOULD be tried, and so on.

	test->addrlen = sizeof(test->addr);
	memset(&test->addr, 0, sizeof(test->addr));
	strcpy(protocol, "UDP");

	uri = uri_parse(destination, strlen(destination));
	if (!uri)
		return -1; // invalid uri

	// rfc3263 4-Client Usage (p5)
	// once a SIP server has successfully been contacted (success is defined below), 
	// all retransmissions of the SIP request and the ACK for non-2xx SIP responses 
	// to INVITE MUST be sent to the same host.
	// Furthermore, a CANCEL for a particular SIP request MUST be sent to the same 
	// SIP server that the SIP request was delivered to.

	// TODO: sips port
	r = socket_addr_from(&test->addr, &test->addrlen, uri->host, uri->port ? uri->port : SIP_PORT);
	if (0 == r)
	{
		socket_addr_to((struct sockaddr*)&test->addr, test->addrlen, ip, &port);
		socket_getname(test->udp, local, &port);
		r = ip_route_get(ip, local);
		if (0 == r)
		{
			dns[0] = 0;
			struct sockaddr_storage ss;
			socklen_t len = sizeof(ss);
			if (0 == socket_addr_from(&ss, &len, local, port))
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
	struct sip_uac_test_t *test = (struct sip_uac_test_t *)transport;

	//char p1[1024];
	//char p2[1024];
	((char*)data)[bytes] = 0;
	printf("%s\n", (const char*)data);
	int r = socket_sendto(test->udp, data, bytes, 0, (struct sockaddr*)&test->addr, test->addrlen);
	return r == bytes ? 0 : -1;
}

static void sip_uac_recv_reply(struct sip_uac_test_t *test)
{
	uint8_t buffer[2 * 1024];
	struct sockaddr_storage addr;
	socklen_t addrlen = sizeof(addr);

	do
	{
		int r = socket_recvfrom(test->udp, buffer, sizeof(buffer), 0, (struct sockaddr*)&addr, &addrlen);

		size_t n = r;
		if (0 == http_parser_input(test->parser, buffer, &n))
		{
			struct sip_message_t* reply = sip_message_create(SIP_MESSAGE_REPLY);
			r = sip_message_load(reply, test->parser);
			assert(0 == sip_uac_input(test->uac, reply));
			sip_message_destroy(reply);

			http_parser_clear(test->parser);
			break;
		}
	} while (1);
}

static inline const char* strlower(char* s)
{
    char* p;
    for(p = s; p && *p; p++)
        *p = tolower(*p);
    return s;
}

static void md5_digest(const uint8_t* data, int size, uint8_t md5[16])
{
	MD5_CTX ctx;
	MD5Init(&ctx);
	MD5Update(&ctx, (unsigned char*)data, size);
	MD5Final(md5, &ctx);
}

static int sip_uac_onregister(void* param, const struct sip_message_t* reply, struct sip_uac_transaction_t* t, int code)
{
	char buffer[1024];
	struct sip_uac_test_t *test = (struct sip_uac_test_t *)param;

	if (200 <= code && code < 300)
	{
	}
	else if (401 == code)
	{
		// https://blog.csdn.net/yunlianglinfeng/article/details/81109380
		// http://www.voidcn.com/article/p-oqqbqgvd-bgn.html
		const char* h;
		t = sip_uac_register(test->uac, "sip:34020000001320000001@192.168.154.1", "sip:192.168.154.128", 3600, sip_uac_onregister, param);
		h = http_get_header_by_name(test->parser, "Call-ID");
		sip_uac_add_header(t, "Call-ID", h); // All registrations from a UAC SHOULD use the same Call-ID
		h = http_get_header_by_name(test->parser, "CSeq");
		snprintf(buffer, sizeof(buffer), "%u REGISTER", atoi(h) + 1);
		sip_uac_add_header(t, "CSeq", buffer); // A UA MUST increment the CSeq value by one for each REGISTER request with the same Call-ID

		char HA1[16], ha1[33] = { 0 };
		char HA2[16], ha2[33] = { 0 };
		// Unauthorized
		struct http_header_www_authenticate_t auth;
		memset(&auth, 0, sizeof(auth));
		h = http_get_header_by_name(test->parser, "WWW-Authenticate");
		assert(0 == http_header_www_authenticate(h, &auth));
		switch (auth.scheme)
		{
		case HTTP_AUTHENTICATION_DIGEST:
			//HA1=md5(username:realm:password)
			//HA2=md5(Method:Uri)
			//RESPONSE=md5(HA1:nonce:HA2)
			snprintf(buffer, sizeof(buffer), "%s:%s:%s", "34020000001320000001", auth.realm, "12345678");
			md5_digest((uint8_t*)buffer, strlen(buffer), (uint8_t*)HA1);
			snprintf(buffer, sizeof(buffer), "%s:%s", "REGISTER", "sip:192.168.154.128");
			md5_digest((uint8_t*)buffer, strlen(buffer), (uint8_t*)HA2);
			base16_encode(ha1, HA1, 16);
			base16_encode(ha2, HA2, 16);
			strlower(ha1);
			strlower(ha2);
			snprintf(buffer, sizeof(buffer), "%s:%s:%s", ha1, auth.nonce, ha2);
			md5_digest((uint8_t*)buffer, strlen(buffer), (uint8_t*)HA2);
			base16_encode(ha2, HA2, 16);
			strlower(ha2);

			snprintf(buffer, sizeof(buffer), "Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", response=\"%s\", algorithm=MD5",
				"34020000001320000001", auth.realm, auth.nonce, "sip:192.168.154.128", ha2);
			sip_uac_add_header(t, "Authorization", buffer);

			assert(0 == sip_uac_send(t, NULL, 0, &test->transport, test));
			sip_uac_recv_reply(test);
			break;

		case HTTP_AUTHENTICATION_BASIC:
			break;
		default:
			assert(0);
		}
	}
	else
	{
	}
	return 0;
}

static void sip_uac_register_test(struct sip_uac_test_t *test)
{
	struct sip_uac_transaction_t* t;
	//t = sip_uac_register(uac, "Bob <sip:bob@biloxi.com>", "sip:registrar.biloxi.com", 7200, sip_uac_message_onregister, test);
	t = sip_uac_register(test->uac, "sip:34020000001320000001@192.168.154.1", "sip:192.168.154.128", 3600, sip_uac_onregister, test);
	assert(0 == sip_uac_send(t, NULL, 0, &test->transport, test));
	sip_uac_recv_reply(test);
}

static int sip_uac_onmessage(void* param, const struct sip_message_t* reply, struct sip_uac_transaction_t* t, int code)
{
	return 0;
}

static void sip_uac_message_test(struct sip_uac_test_t *test)
{
	const char* msg = "<?xml version=\"1.0\"?>"
						"<Notify>"
						"<CmdType>Keepalive</CmdType>"
						"<SN>478</SN>"
						"<DeviceID>34020000001320000001</DeviceID>"
						"<Status>OK</Status>"
						"</Notify>";

	struct sip_uac_transaction_t* t;
	t = sip_uac_message(test->uac, "sip:34020000001320000001@192.168.154.1", "sip:34020000002000000001@192.168.154.128", sip_uac_onmessage, test);
	sip_uac_add_header(t, "Content-Type", "Application/MANSCDP+xml");
	assert(0 == sip_uac_send(t, msg, strlen(msg), &test->transport, test));
	sip_uac_recv_reply(test);
}

static int sip_uac_oninvited(void* param, const struct sip_message_t* reply, struct sip_uac_transaction_t* t, struct sip_dialog_t* dialog, int code)
{
	return 0;
}

static void sip_uac_invite_test(struct sip_uac_test_t *test)
{
	struct sip_uac_transaction_t* t;
	t = sip_uac_invite(test->uac, "sip:34020000001320000001@192.168.154.128", "sip:34020000001320000001@192.168.154.128", sip_uac_oninvited, test);
	assert(0 == sip_uac_send(t, NULL, 0, &test->transport, test));
	sip_uac_recv_reply(test);
}

static int STDCALL TimerThread(void* param)
{
	bool *running = (bool*)param;
	while (*running)
	{
		aio_timeout_process();
		system_sleep(5);
	}
	return 0;
}

void sip_uac_test(void)
{
	struct sip_uac_test_t test;
	test.transport = {
		sip_uac_transport_via,
		sip_uac_transport_send,
	};

	pthread_t th;
	bool running = true;
	thread_create(&th, TimerThread, &running);

	test.udp = socket_udp();
	test.uac = sip_uac_create();
	test.parser = http_parser_create(HTTP_PARSER_CLIENT);
	socket_bind_any(test.udp, SIP_PORT);
	sip_uac_register_test(&test);
	sip_uac_message_test(&test);
	//sip_uac_invite_test(&test);

	running = false;
	thread_destroy(th);

	sip_uac_destroy(test.uac);
	socket_close(test.udp);
	http_parser_destroy(test.parser);
}
