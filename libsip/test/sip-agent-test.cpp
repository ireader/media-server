#include "sockutil.h"
#include "aio-timeout.h"
#include "sip-agent.h"
#include "sip-uac.h"
#include "sip-uas.h"
#include "sip-message.h"
#include "sip-transport.h"
#include "port/ip-route.h"
#include "http-parser.h"
#include "http-header-auth.h"
#include "sys/thread.h"
#include "sys/system.h"
#include "sys/sync.hpp"
#include "time64.h"
#include "channel.h"
#include "uri-parse.h"
#include "cstringext.h"
#include "base64.h"
#include "md5.h"
#include <stdint.h>

#define NQUEUE 3
#define NCONCURRENT 200
#define CHANNEL_DONE ((void*)0xabcdef98)

struct sip_tu_t
{
    struct sip_agent_t* sip;
    struct channel_t* q[NQUEUE];
    struct sip_transport_t transport;
    int32_t terminated;
};

struct sip_agent_test_t
{
	socket_t udp;
	bool running;
	struct sip_tu_t alice;
	struct sip_tu_t bob;
};

struct sip_packet_t
{
	void* ptr;
	int len;
};

struct sip_task_t
{
    struct sip_tu_t* self;
    struct sip_tu_t* peer;
	char from[64];
	char to[64];

    ThreadEvent event;
	struct sip_dialog_t* dialog;
    int32_t terminated;
    int32_t success;
	int32_t failed;
};
static struct sip_task_t s_alice_tasks[NCONCURRENT];
static struct sip_task_t s_bob_tasks[NCONCURRENT];
static struct sip_agent_test_t s_sip;

static int sip_uac_transport_via(void* transport, const char* destination, char protocol[16], char local[128], char dns[128])
{
	int r;
	char ip[65];
	u_short port;
	struct uri_t* uri;
	socklen_t addrlen;
	struct sockaddr_storage addr;

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

	addrlen = sizeof(addr);
	memset(&addr, 0, sizeof(addr));
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
	r = socket_addr_from(&addr, &addrlen, uri->host, uri->port ? uri->port : SIP_PORT);
	if (0 == r)
	{
		socket_addr_to((struct sockaddr*)&addr, addrlen, ip, &port);
		socket_getname(s_sip.udp, local, &port);
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
        else
        {
            assert(0);
        }
	}
    else
    {
        assert(0);
    }

	uri_free(uri);
	return r;
}

static int sip_uac_transport_send(void* param, const void* data, size_t bytes)
{
	struct sip_task_t *task = (struct sip_task_t*)param;

	if ((rand() % 100) < 20)
		return 0; // packet lost

	int i = rand() % NQUEUE;
	sip_packet_t pkt;
	pkt.ptr = malloc(bytes);
	pkt.len = bytes;
	memcpy(pkt.ptr, data, bytes);

	char st[32];
	time64_format(time64_now(), "%04Y-%02M-%02D %02h:%02m:%02s", st);
	//printf("UAC: %s\n%.*s\n", st, bytes, (const char*)pkt.ptr);

	assert(0 == channel_push(task->peer->q[i], &pkt));
	return 0;
}

static int sip_uas_transport_send(void* param, const struct cstring_t* url, const void* data, int bytes)
{
    assert(param == &s_sip.alice || param == &s_sip.bob);
    struct sip_tu_t* tu = (struct sip_tu_t*)param;
    struct sip_tu_t* peer = tu == &s_sip.alice ? &s_sip.bob : &s_sip.alice;

	if ((rand() % 100) < 20)
		return 0; // packet lost

	sip_packet_t pkt;
	pkt.ptr = malloc(bytes);
	pkt.len = bytes;
	memcpy(pkt.ptr, data, bytes);

	char st[32];
	time64_format(time64_now(), "%04Y-%02M-%02D %02h:%02m:%02s", st);
	//printf("UAS: %s\n%.*s\n", st, bytes, (const char*)pkt.ptr);

    int i = rand() % NQUEUE;
    assert(0 == channel_push(peer->q[i], &pkt));
	return 0;
}

static void sip_uac_transaction_ondestroy(void* param)
{
    struct sip_task_t *task = (struct sip_task_t *)param;
    atomic_decrement32(&task->terminated);
}
static int sip_uac_onmessage(void* param, const struct sip_message_t* reply, struct sip_uac_transaction_t* t, int code)
{
	assert(code >= 200 && code < 700);
	struct sip_task_t *task = (struct sip_task_t *)param;
	task->success += (code >= 200 && code < 300);
	task->failed += (code >= 300 && code < 700);
	task->event.Signal();
	return 0;
}

static void sip_uac_message_test(struct sip_task_t *task)
{
	const char* msg = "<?xml version=\"1.0\"?><msg>Hello SIP</msg>";
	struct sip_uac_transaction_t* t;
	t = sip_uac_message(task->self->sip, task->from, task->to, sip_uac_onmessage, task);
    sip_uac_transaction_ondestroy(t, sip_uac_transaction_ondestroy, task);
	sip_uac_add_header(t, "Content-Type", "Application/xml");
	assert(0 == sip_uac_send(t, msg, strlen(msg), &task->self->transport, task));
	assert(0 == task->event.Wait());
}

static int sip_uac_onregister(void* param, const struct sip_message_t* reply, struct sip_uac_transaction_t* t, int code)
{
	assert(code >= 200 && code < 700);
	struct sip_task_t *task = (struct sip_task_t *)param;
	task->success += (code >= 200 && code < 300);
	task->failed += (code >= 300 && code < 700);
	task->event.Signal();
	return 0;
}

static void sip_uac_register_test(struct sip_task_t *task)
{
	struct sip_uac_transaction_t* t;
	t = sip_uac_register(task->self->sip, task->from, "sip:127.0.0.1", 7200, sip_uac_onregister, task);
    sip_uac_transaction_ondestroy(t, sip_uac_transaction_ondestroy, task);
	assert(0 == sip_uac_send(t, NULL, 0, &task->self->transport, task));
	assert(0 == task->event.Wait());
}

static void* sip_uac_oninvited(void* param, const struct sip_message_t* reply, struct sip_uac_transaction_t* t, struct sip_dialog_t* dialog, int code)
{
	assert(code >= 100 && code < 700);
	if (code >= 200 && code < 700)
	{
		struct sip_task_t *task = (struct sip_task_t *)param;
		task->success += (code >= 200 && code < 300);
		task->failed += (code >= 300 && code < 700);
		assert(task->dialog == NULL);
		task->dialog = dialog;
		task->event.Signal();
	}
	return NULL;
}

static void sip_uac_invite_test(struct sip_task_t *task)
{
	assert(task->dialog == NULL);
	struct sip_uac_transaction_t* t;
	t = sip_uac_invite(task->self->sip, task->from, task->to, sip_uac_oninvited, task);
    sip_uac_transaction_ondestroy(t, sip_uac_transaction_ondestroy, task);
	assert(0 == sip_uac_send(t, NULL, 0, &task->self->transport, task));
	assert(0 == task->event.Wait());
}

static int sip_uac_onbye(void* param, const struct sip_message_t* reply, struct sip_uac_transaction_t* t, int code)
{
	assert(code >= 200 && code < 700);
	struct sip_task_t *task = (struct sip_task_t *)param;
	task->success += (code >= 200 && code < 300);
	task->failed += (code >= 300 && code < 700);
	assert(task->dialog);
	if ((code >= 200 && code < 300) || 481 == code)
		task->dialog = NULL;
	else
		printf("%s bye failed: %d\n", task->from, code);
	task->event.Signal();
	return 0;
}

static void sip_uac_bye_test(struct sip_task_t *task)
{
	assert(task->dialog);
	struct sip_uac_transaction_t* t;
	t = sip_uac_bye(task->self->sip, task->dialog, sip_uac_onbye, task);
    sip_uac_transaction_ondestroy(t, sip_uac_transaction_ondestroy, task);
	assert(0 == sip_uac_send(t, NULL, 0, &task->self->transport, task));
	assert(0 == task->event.Wait());
}

static int STDCALL AliceThread(void* param)
{
	struct sip_task_t* task = (struct sip_task_t*)param;

	for (int i = 0; i < 100; i++)
	{
		switch (rand() % 3)
		{
		case 0:
            atomic_increment32(&task->terminated);
			sip_uac_register_test(task);
		case 1:
            atomic_increment32(&task->terminated);
			sip_uac_message_test(task);
		case 2:
            atomic_increment32(&task->terminated);
			sip_uac_invite_test(task);
			while(task->dialog)
            {
                atomic_increment32(&task->terminated);
				sip_uac_bye_test(task);
            }
		}
	}
    
    while(task->terminated != 0)
        system_sleep(10);
    printf("%s done\n", task->from);
	return 0;
}

static int STDCALL BobThread(void* param)
{
	struct sip_task_t* task = (struct sip_task_t*)param;
	
	for (int i = 0; i < 100; i++)
	{
		switch (rand() % 3)
		{
		case 0:
            atomic_increment32(&task->terminated);
			sip_uac_register_test(task);
		case 1:
            atomic_increment32(&task->terminated);
			sip_uac_message_test(task);
		case 2:
            atomic_increment32(&task->terminated);
			sip_uac_invite_test(task);
			while (task->dialog)
            {
                atomic_increment32(&task->terminated);
                sip_uac_bye_test(task);
            }
		}
	}
    
    while(task->terminated != 0)
        system_sleep(10);
    printf("%s done\n", task->from);
	return 0;
}

static void sip_uas_transaction_ondestroy(void* param)
{
    struct sip_tu_t *tu = (struct sip_tu_t *)param;
    assert(atomic_decrement32(&tu->terminated) >= 0);
}

static void* sip_uas_oninvite(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, struct sip_dialog_t* dialog, const void* data, int bytes)
{
	char contact[128];
    struct sip_tu_t* tu = (struct sip_tu_t*)param;
    atomic_increment32(&tu->terminated);
    sip_uas_transaction_ondestroy(t, sip_uas_transaction_ondestroy, param);
	sip_contact_write(&req->to, contact, contact+sizeof(contact));
	sip_uas_add_header(t, "Contact", contact);
	assert(0 == sip_uas_reply(t, 200, NULL, 0));
	return t;
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
    struct sip_tu_t* tu = (struct sip_tu_t*)param;
    atomic_increment32(&tu->terminated);
    sip_uas_transaction_ondestroy(t, sip_uas_transaction_ondestroy, param);
	return sip_uas_reply(t, 200, NULL, 0);
}

/// cancel a transaction(should be an invite transaction)
static int sip_uas_oncancel(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session)
{
    struct sip_tu_t* tu = (struct sip_tu_t*)param;
    atomic_increment32(&tu->terminated);
    sip_uas_transaction_ondestroy(t, sip_uas_transaction_ondestroy, param);
	return sip_uas_reply(t, 200, NULL, 0);
}

/// @param[in] expires in seconds
static int sip_uas_onregister(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, const char* user, const char* location, int expires)
{
    struct sip_tu_t* tu = (struct sip_tu_t*)param;
    atomic_increment32(&tu->terminated);
    sip_uas_transaction_ondestroy(t, sip_uas_transaction_ondestroy, param);
	return sip_uas_reply(t, 200, NULL, 0);
}

static int sip_uas_onmessage(void* param, const struct sip_message_t* req, struct sip_uas_transaction_t* t, void* session, const void* payload, int bytes)
{
    struct sip_tu_t* tu = (struct sip_tu_t*)param;
    atomic_increment32(&tu->terminated);
    sip_uas_transaction_ondestroy(t, sip_uas_transaction_ondestroy, param);
	return sip_uas_reply(t, 200, NULL, 0);
}

static int STDCALL InputThread(struct sip_tu_t* tu, int idx)
{
	http_parser_t* request = http_parser_create(HTTP_PARSER_RESPONSE, NULL, NULL);
	http_parser_t* response = http_parser_create(HTTP_PARSER_REQUEST, NULL, NULL);

	while(channel_count(tu->q[idx]) > 0 || s_sip.running)
	{
		//int r = socket_recvfrom(test->udp, buffer, sizeof(buffer), 0, (struct sockaddr*)&addr, &addrlen);
		sip_packet_t pkt;
		memset(&pkt, 0, sizeof(pkt));
		assert(0 == (sip_packet_t*)channel_pop(tu->q[idx], &pkt));
        if(pkt.ptr == CHANNEL_DONE)
            continue;

		http_parser_t* parser = 0 == strncasecmp("SIP", (char*)pkt.ptr, 3) ? request : response;

		size_t n = pkt.len;
		assert(0 == http_parser_input(parser, pkt.ptr, &n));
		struct sip_message_t* msg = sip_message_create(parser==response? SIP_MESSAGE_REQUEST : SIP_MESSAGE_REPLY);
		assert(0 == sip_message_load(msg, parser));
		assert(0 == sip_agent_input(tu->sip, msg));
		sip_message_destroy(msg);
		http_parser_clear(parser);

		free(pkt.ptr);
	}

	http_parser_destroy(request);
	http_parser_destroy(response);
	return 0;
}

static int STDCALL AliceInputThread(void* param)
{
    return InputThread(&s_sip.alice, (int)(intptr_t)param);
}
static int STDCALL BobInputThread(void* param)
{
    return InputThread(&s_sip.bob, (int)(intptr_t)param);
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

extern "C" void sip_agent_test(void)
{
	s_sip.alice.transport = {
		sip_uac_transport_via,
		sip_uac_transport_send,
	};
    s_sip.bob.transport = {
        sip_uac_transport_via,
        sip_uac_transport_send,
    };
    s_sip.running = true;

	struct sip_uas_handler_t handler;
	handler.onregister = sip_uas_onregister;
	handler.oninvite = sip_uas_oninvite;
	handler.onack = sip_uas_onack;
	handler.onbye = sip_uas_onbye;
	handler.oncancel = sip_uas_oncancel;
	handler.onmessage = sip_uas_onmessage;
	handler.send = sip_uas_transport_send;

	s_sip.udp = socket_udp();
	s_sip.alice.sip = sip_agent_create(&handler, &s_sip.alice);
	s_sip.bob.sip = sip_agent_create(&handler, &s_sip.bob);
    s_sip.alice.terminated = 0;
    s_sip.bob.terminated = 0;

	pthread_t timer[NQUEUE];
	pthread_t worker[NQUEUE*2];
	for (int i = 0; i < NQUEUE; i++)
	{
		s_sip.alice.q[i] = channel_create(200, sizeof(struct sip_packet_t));
		s_sip.bob.q[i] = channel_create(200, sizeof(struct sip_packet_t));
		thread_create(&timer[i], TimerThread, &s_sip.running);
		thread_create(&worker[2*i], AliceInputThread, (void*)(intptr_t)i); // alice
		thread_create(&worker[2*i+1], BobInputThread, (void*)(intptr_t)i); // bob
	}

	pthread_t bob[NCONCURRENT];
	pthread_t alice[NCONCURRENT];
	for (int i = 0; i < NCONCURRENT; i++)
	{
        s_alice_tasks[i].dialog = NULL;
        s_alice_tasks[i].self = &s_sip.alice;
        s_alice_tasks[i].peer = &s_sip.bob;
        s_alice_tasks[i].success = 0;
        s_alice_tasks[i].failed = 0;
        s_alice_tasks[i].terminated = 0;
        snprintf(s_alice_tasks[i].from, sizeof(s_alice_tasks[i].from), "sip:alice%03d@127.0.0.1", i);
        snprintf(s_alice_tasks[i].to, sizeof(s_alice_tasks[i].to), "sip:bob%03d@127.0.0.1", i);
		thread_create(&alice[i], AliceThread, &s_alice_tasks[i]);
        
        s_bob_tasks[i].dialog = NULL;
        s_bob_tasks[i].self = &s_sip.bob;
        s_bob_tasks[i].peer = &s_sip.alice;
        s_bob_tasks[i].success = 0;
        s_bob_tasks[i].failed = 0;
        s_bob_tasks[i].terminated = 0;
        snprintf(s_bob_tasks[i].from, sizeof(s_bob_tasks[i].from), "sip:BOB%03d@127.0.0.1", i);
        snprintf(s_bob_tasks[i].to, sizeof(s_bob_tasks[i].to), "sip:ALICE%03d@127.0.0.1", i);
		thread_create(&bob[i], BobThread, &s_bob_tasks[i]);
	}

	// do worker
	for (int i = 0; i < NCONCURRENT; i++)
	{
		thread_destroy(alice[i]);
		thread_destroy(bob[i]);
	}

	s_sip.running = false;
	for (int i = 0; i < NQUEUE; i++)
	{
        sip_packet_t pkt;
        memset(&pkt, 0, sizeof(pkt));
        pkt.ptr = CHANNEL_DONE;
        channel_push(s_sip.alice.q[i], &pkt);
        channel_push(s_sip.bob.q[i], &pkt);
		thread_destroy(worker[2 * i]);
		thread_destroy(worker[2 * i + 1]);
	}

	for (int i = 0; i < NQUEUE; i++)
	{
		assert(0 == channel_count(s_sip.alice.q[i]));
        assert(0 == channel_count(s_sip.bob.q[i]));
		channel_destroy(&s_sip.alice.q[i]);
        channel_destroy(&s_sip.bob.q[i]);
	}

    while(s_sip.alice.terminated != 0 || s_sip.bob.terminated != 0)
        system_sleep(10);
    
    for (int i = 0; i < NQUEUE; i++)
        thread_destroy(timer[i]);

	assert(0 == sip_agent_destroy(s_sip.alice.sip));
	assert(0 == sip_agent_destroy(s_sip.bob.sip));
	socket_close(s_sip.udp);
}
