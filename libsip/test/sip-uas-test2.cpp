#include <map>
#include <memory>
#include <string>
#include "sockutil.h"
#include "sockpair.h"
#include "sys/thread.h"
#include "sys/system.h"
#include "sip-uac.h"
#include "sip-uas.h"
#include "sip-message.h"
#include "sip-transport.h"
#include "port/ip-route.h"
#include "http-parser.h"
#include "http-header-auth.h"
#include "rtsp-media.h"
#include "../test/media/pcm-file-source.h"
#include "../test/media/h264-file-source.h"
#include "../test/rtp-udp-transport.h"
#include "uri-parse.h"
#include "cstringext.h"
#include "base64.h"
#include "sdp.h"
#include "md5.h"
#include <stdint.h>
#include <errno.h>

#define NAME "tao3"
#define HOST "192.168.3.34"

struct sip_uas_test_t
{
	socket_t udp;
	socklen_t addrlen;
	struct sockaddr_storage addr;

	http_parser_t* request;
	http_parser_t* response;
	struct sip_agent_t* sip;
};

struct sip_media_t
{
	struct rtsp_media_t medias[3];
	int nmedia;

	std::shared_ptr<IRTPTransport> transport;
	std::shared_ptr<IMediaSource> source;
	unsigned short port[2];
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
	const char* pattern = "v=0\n"
		"o=- 0 0 IN IP4 %s\n"
		"s=Play\n"
		"c=IN IP4 %s\n"
		"t=0 0\n"
		"m=audio %hu RTP/AVP 8\n"
		//"a=rtpmap:96 PS/90000\n"
		//"a=fmtp:98 profile-level-id=42800D;packetization-mode=1;sprop-parameter-sets=Z0KADYiLULBLQgAAIygAAr8gCAAAAAAB,aM44gAAAAAE=\n"
		;

	char reply[1024];
	const cstring_t* h = sip_message_get_header_by_name(req, "Content-Type");
	if (0 == cstrcasecmp(h, "Application/SDP"))
	{
		socklen_t len = 0;
		struct sip_media_t* m = new sip_media_t;
		m->transport.reset(new RTPUdpTransport());
		m->nmedia = rtsp_media_sdp((const char*)data, m->medias, sizeof(m->medias) / sizeof(m->medias[0]));
		assert(m->nmedia > 0);
		assert(0 == strcasecmp("IP4", m->medias[0].addrtype) || 0 == strcasecmp("IP6", m->medias[0].addrtype));
		m->port[0] = m->medias[0].port[0];
		m->port[1] = m->medias[0].nport > 1 ? m->medias[0].port[1] : (m->medias[0].port[0] + 1);
		assert(0 == ((RTPUdpTransport*)m->transport.get())->Init(m->medias[0].address, m->port));
		m->source.reset(new PCMFileSource("C:\\Users\\Administrator\\sintel-1280.pcm"));
		std::string sdp;
		m->source->GetSDPMedia(sdp);

		sip_uas_add_header(t, "Content-Type", "application/sdp");
		sip_uas_add_header(t, "Contact", "sip:" NAME "@" HOST);
		snprintf(reply, sizeof(reply), pattern, HOST, HOST, m->port[0]);
		assert(0 == sip_uas_reply(t, 200, reply, strlen(reply)));
		return m;
	}
	else
	{
		assert(0);
		return NULL;
	}
}

static int STDCALL rtsp_play_thread(void* param)
{
	struct sip_media_t* m = (struct sip_media_t*)param;
	m->source->SetTransport("track1", m->transport);
	while (1)
	{
		m->source->Play();
		system_sleep(10);
	}

	delete m;
}

/// @param[in] code 0-ok, other-sip status code
/// @return 0-ok, other-error
static int sip_uas_onack(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session, struct sip_dialog_t* dialog, int code, const void* data, int bytes)
{
	struct sip_media_t* m = (struct sip_media_t*)session;

	if (200 <= code && code < 300)
	{
		pthread_t th;
		thread_create(&th, rtsp_play_thread, m);
	}
	else
	{
		delete m;
		assert(0);
	}
	return 0;
}

/// on terminating a session(dialog)
static int sip_uas_onbye(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session)
{
	return sip_uas_reply(t, 200, NULL, 0);
}

/// cancel a transaction(should be an invite transaction)
static int sip_uas_oncancel(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session)
{
	return sip_uas_reply(t, 200, NULL, 0);
}

/// @param[in] expires in seconds
static int sip_uas_onregister(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, const char* user, const char* location, int expires)
{
	return sip_uas_reply(t, 200, NULL, 0);
}

static void sip_uas_loop(struct sip_uas_test_t *test)
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
		parser = 0 == strncasecmp("SIP", (char*)buffer, 3) ? test->request: test->response;

        size_t n = r;
        assert(0 == http_parser_input(parser, buffer, &n));
        struct sip_message_t* msg = sip_message_create(parser==test->response? SIP_MESSAGE_REQUEST : SIP_MESSAGE_REPLY);
        assert(0 == sip_message_load(msg, parser));
        assert(0 == sip_agent_input(test->sip, msg));
        sip_message_destroy(msg);
        http_parser_clear(parser);
	} while (1);
}

void sip_uas_test2(void)
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
	test.request = http_parser_create(HTTP_PARSER_RESPONSE, NULL, NULL);
	test.response = http_parser_create(HTTP_PARSER_REQUEST, NULL, NULL);
	socket_bind_any(test.udp, SIP_PORT);
	sip_uas_loop(&test);
	sip_agent_destroy(test.sip);
	socket_close(test.udp);
	http_parser_destroy(test.request);
	http_parser_destroy(test.response);
}
