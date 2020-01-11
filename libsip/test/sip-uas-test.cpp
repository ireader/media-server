#include "sockutil.h"
#include "sip-uac.h"
#include "sip-uas.h"
#include "sip-message.h"
#include "sip-transport.h"
#include "port/ip-route.h"
#include "http-parser.h"
#include "http-header-auth.h"
#include "uri-parse.h"
#include "cstringext.h"
#include "base64.h"
#include "sdp.h"
#include "md5.h"
#include <stdint.h>

#define NAME "34020000001320000001"
#define DOMAIN "192.168.154.1"

struct sip_uas_test_t
{
	socket_t udp;
	socklen_t addrlen;
	struct sockaddr_storage addr;

	http_parser_t* parser;
	struct sip_agent_t* sip;
};

static int sip_uas_transport_send(void* param, const struct cstring_t* url, const void* data, int bytes)
{
	struct sip_uas_test_t *test = (struct sip_uas_test_t *)param;

	//char p1[1024];
	//char p2[1024];
	((char*)data)[bytes] = 0;
	printf("%s\n\n", (const char*)data);
	int r = socket_sendto(test->udp, data, bytes, 0, (struct sockaddr*)&test->addr, test->addrlen);
	return r == bytes ? 0 : -1;
}

static void* sip_uas_oninvite(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, struct sip_dialog_t* dialog, const void* data, int bytes)
{
	const char* ack = "v=0\n"
		"o=34020000001320000001 0 0 IN IP4 192.168.128.1\n"
		"s=Play\n"
		"c=IN IP4 192.168.128.1\n"
		"t=0 0\n"
		"m=video 20120 RTP/AVP 96 98 97\n"
		"a=sendonly\n"
		"a=rtpmap:96 PS/90000\n"
		"a=rtpmap:98 H264/90000\n"
		"a=rtpmap:97 MPEG4/90000\n"
		"y=0100000001\n"
		"f=v/2/4///a///\n";

	const cstring_t* h = sip_message_get_header_by_name(req, "Content-Type");
	if (0 == cstrcasecmp(h, "Application/SDP"))
	{
		sdp_t* sdp = sdp_parse((const char*)data);
		sip_uas_add_header(t, "Content-Type", "application/sdp");
		sip_uas_add_header(t, "Contact", "sip:34020000001320000001@192.168.154.1");
		assert(0 == sip_uas_reply(t, 200, ack, strlen(ack)));
		sdp_destroy(sdp);
		return t;
	}
	else
	{
		assert(0);
		return t;
	}
}

/// @param[in] code 0-ok, other-sip status code
/// @return 0-ok, other-error
static int sip_uas_onack(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session, struct sip_dialog_t* dialog, int code, const void* data, int bytes)
{
	return 0;
}

/// on terminating a session(dialog)
static int sip_uas_onbye(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session)
{
	return 0;
}

/// cancel a transaction(should be an invite transaction)
static int sip_uas_oncancel(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session)
{
	return 0;
}

/// @param[in] expires in seconds
static int sip_uas_onregister(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, const char* user, const char* location, int expires)
{
	return 0;
}

static void sip_uas_loop(struct sip_uas_test_t *test)
{
	uint8_t buffer[2 * 1024];

	do
	{
		memset(buffer, 0, sizeof(buffer));
		test->addrlen = sizeof(test->addr);
		int r = socket_recvfrom(test->udp, buffer, sizeof(buffer), 0, (struct sockaddr*)&test->addr, &test->addrlen);
		printf("\n%s\n", buffer);

		size_t n = r;
		if (0 == http_parser_input(test->parser, buffer, &n))
		{
			struct sip_message_t* reply = sip_message_create(SIP_MESSAGE_REQUEST);
			r = sip_message_load(reply, test->parser);
			assert(0 == sip_agent_input(test->sip, reply));
			sip_message_destroy(reply);

			http_parser_clear(test->parser);
		}
	} while (1);
}

void sip_uas_test(void)
{
	struct sip_uas_handler_t handler;
	handler.onregister = sip_uas_onregister;
	handler.oninvite = sip_uas_oninvite;
	handler.onack = sip_uas_onack;
	handler.onbye = sip_uas_onbye;
	handler.oncancel = sip_uas_oncancel;
	handler.send = sip_uas_transport_send;

	struct sip_uas_test_t test;
	test.udp = socket_udp();
	test.sip = sip_agent_create(&handler, &test);
	test.parser = http_parser_create(HTTP_PARSER_REQUEST, NULL, NULL);
	socket_bind_any(test.udp, SIP_PORT);
	sip_uas_loop(&test);
	sip_agent_destroy(test.sip);
	socket_close(test.udp);
	http_parser_destroy(test.parser);
}
