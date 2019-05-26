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
#include <errno.h>

#define SIP_USR "sipuser1"
#define SIP_PWD "1234567890"
#define SIP_HOST "sip.linphone.org"
#define SIP_FROM "sip:sipuser1@sip.linphone.org"
#define SIP_PEER "sip:sipuser2@sip.linphone.org"

struct sip_uac_test2_t
{
	socket_t udp;
	socklen_t addrlen;
	struct sockaddr_storage addr;
    bool running;

    http_parser_t* request;
    http_parser_t* response;
	struct sip_agent_t* sip;
	struct sip_transport_t transport;
	struct http_header_www_authenticate_t auth;
	int nonce_count;
};

static int sip_uac_transport_via(void* transport, const char* destination, char protocol[16], char local[128], char dns[128])
{
	int r;
	char ip[65];
	u_short port;
	struct uri_t* uri;

	struct sip_uac_test2_t *test = (struct sip_uac_test2_t *)transport;

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
	struct sip_uac_test2_t *test = (struct sip_uac_test2_t *)transport;

	//char p1[1024];
	//char p2[1024];
	((char*)data)[bytes] = 0;
	printf("%s\n", (const char*)data);
	int r = socket_sendto(test->udp, data, bytes, 0, (struct sockaddr*)&test->addr, test->addrlen);
	return r == bytes ? 0 : -1;
}

static int sip_uac_oninvited(void* param, const struct sip_message_t* reply, struct sip_uac_transaction_t* t, struct sip_dialog_t* dialog, int code)
{
    return 0;
}

static void sip_uac_invite_test(struct sip_uac_test2_t *test)
{
    char buffer[1024];
    struct sip_uac_transaction_t* t;
    t = sip_uac_invite(test->sip, SIP_FROM, SIP_PEER, sip_uac_oninvited, test);
    ++test->auth.nc;
    snprintf(test->auth.uri, sizeof(test->auth.uri), "%s", SIP_PEER);
    snprintf(test->auth.username, sizeof(test->auth.username), "%s", SIP_USR);
    http_header_auth(&test->auth, SIP_PWD, "INVITE", NULL, 0, buffer, sizeof(buffer));
    sip_uac_add_header(t, "Proxy-Authorization", buffer);
    assert(0 == sip_uac_send(t, NULL, 0, &test->transport, test));
}

static int sip_uac_onregister(void* param, const struct sip_message_t* reply, struct sip_uac_transaction_t* t, int code)
{
	char buffer[1024];
	struct sip_uac_test2_t *test = (struct sip_uac_test2_t *)param;

	if (200 <= code && code < 300)
	{
        sip_uac_invite_test(test);
	}
	else if (401 == code)
	{
		// https://blog.csdn.net/yunlianglinfeng/article/details/81109380
		// http://www.voidcn.com/article/p-oqqbqgvd-bgn.html
		const char* h;
		t = sip_uac_register(test->sip, SIP_FROM, SIP_PEER, 3600, sip_uac_onregister, param);
		h = http_get_header_by_name(test->request, "Call-ID");
		sip_uac_add_header(t, "Call-ID", h); // All registrations from a UAC SHOULD use the same Call-ID
		h = http_get_header_by_name(test->request, "CSeq");
		snprintf(buffer, sizeof(buffer), "%u REGISTER", atoi(h) + 1);
		sip_uac_add_header(t, "CSeq", buffer); // A UA MUST increment the CSeq value by one for each REGISTER request with the same Call-ID

		// Unauthorized
		memset(&test->auth, 0, sizeof(test->auth));
		h = http_get_header_by_name(test->request, "WWW-Authenticate");
		assert(0 == http_header_www_authenticate(h, &test->auth));
		test->nonce_count = 0;
		switch (test->auth.scheme)
		{
		case HTTP_AUTHENTICATION_DIGEST:
			++test->auth.nc;
			snprintf(test->auth.uri, sizeof(test->auth.uri), "sip:%s", SIP_HOST);
			snprintf(test->auth.username, sizeof(test->auth.username), "%s", SIP_USR);
			http_header_auth(&test->auth, SIP_PWD, "REGISTER", NULL, 0, buffer, sizeof(buffer));
			sip_uac_add_header(t, "Authorization", buffer);
			assert(0 == sip_uac_send(t, NULL, 0, &test->transport, test));
			break;

		case HTTP_AUTHENTICATION_BASIC:
			assert(0);
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

static void sip_uac_register_test(struct sip_uac_test2_t *test)
{
	struct sip_uac_transaction_t* t;
	//t = sip_uac_register(uac, "Bob <sip:bob@biloxi.com>", "sip:registrar.biloxi.com", 7200, sip_uac_message_onregister, test);
	t = sip_uac_register(test->sip, SIP_FROM, "sip:" SIP_HOST, 3600, sip_uac_onregister, test);
	assert(0 == sip_uac_send(t, NULL, 0, &test->transport, test));
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

static int sip_uac_test_process(struct sip_uac_test2_t* test)
{
    uint8_t buffer[2 * 1024];
    http_parser_t* parser;
    
    do
    {
        memset(buffer, 0, sizeof(buffer));
        test->addrlen = sizeof(test->addr);
        int r = socket_recvfrom(test->udp, buffer, sizeof(buffer), 0, (struct sockaddr*)&test->addr, &test->addrlen);
        if(-1 == r && EINTR == errno)
            continue;
        
        printf("\n%s\n", buffer);
        parser = 0 == strncasecmp("SIP", (char*)buffer, 3) ? test->response : test->request;
        
        size_t n = r;
        assert(0 == http_parser_input(parser, buffer, &n));
        struct sip_message_t* msg = sip_message_create(parser==test->response? SIP_MESSAGE_REQUEST : SIP_MESSAGE_REPLY);
        assert(0 == sip_message_load(msg, parser));
        assert(0 == sip_agent_input(test->sip, msg));
        sip_message_destroy(msg);
        http_parser_clear(parser);
    } while (1);
    
    return 0;
}

void sip_uac_test2(void)
{
	socket_init();
	struct sip_uac_test2_t test;
	test.transport = {
		sip_uac_transport_via,
		sip_uac_transport_send,
	};
    test.running = true;
    struct sip_uas_handler_t handler;
	memset(&handler, 0, sizeof(handler));

	pthread_t th;
	thread_create(&th, TimerThread, &test.running);

	test.udp = socket_udp();
	test.sip = sip_agent_create(&handler, &test);
    test.request = http_parser_create(HTTP_PARSER_CLIENT);
    test.response = http_parser_create(HTTP_PARSER_SERVER);
	socket_bind_any(test.udp, SIP_PORT);
	sip_uac_register_test(&test);
    
    sip_uac_test_process(&test);

	thread_destroy(th);
	sip_agent_destroy(test.sip);
	socket_close(test.udp);
    http_parser_destroy(test.request);
    http_parser_destroy(test.response);
	socket_cleanup();
}
