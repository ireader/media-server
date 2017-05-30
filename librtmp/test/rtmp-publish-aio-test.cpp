#include "sys/sock.h"
#include "sys/system.h"
#include "sys/sync.hpp"
#include "rtmp-client-transport.h"
#include "flv-reader.h"
#include "avpacket.h"
#include "time64.h"
#include <stdio.h>
#include <assert.h>
#include <list>

static ThreadLocker s_locker;
static std::list<struct avpacket_t*> s_packets;
static bool s_sending = false;
static void* s_transport;

static void rtmp_client_publish_send(void* transport)
{
	if (!s_sending && !s_packets.empty())
	{
		s_sending = true;
		avpacket_t* pkt = s_packets.front();
		int r = rtmp_client_transport_sendpacket(transport, pkt->pic_type, pkt->data, pkt->bytes, (uint32_t)pkt->dts);
		if (0 != r)
		{
			printf("rtmp_client_publish_send: %d/%d\n", r, socket_geterror());
		}
	}
}

static void rtmp_client_push(const char* flv, void* transport)
{
	int r, type;
	uint32_t timestamp;
	static uint32_t s_timestamp = 0;
	void* f = flv_reader_create(flv);

	static char packet[2 * 1024 * 1024];
	while ((r = flv_reader_read(f, &type, &timestamp, packet, sizeof(packet))) > 0)
	{
		if (timestamp > s_timestamp)
			system_sleep(timestamp - s_timestamp);
		s_timestamp = timestamp;

		avpacket_t* pkt = avpacket_alloc(r);
		memcpy(pkt->data, packet, r);
		pkt->bytes = r;
		pkt->pic_type = 8 == type ? 0 : 1;
		pkt->pts = timestamp;
		pkt->dts = timestamp;

		AutoThreadLocker locker(s_locker);
		s_packets.push_back(pkt);
		rtmp_client_publish_send(transport);
	}

	flv_reader_destroy(f);
}

static void rtmp_client_publish_onready(void*)
{
	AutoThreadLocker locker(s_locker);
	rtmp_client_publish_send(s_transport);
}

static void rtmp_client_publish_onsend(void*)
{
	AutoThreadLocker locker(s_locker);
	avpacket_t* pkt = s_packets.front();
	s_packets.pop_front();
	avpacket_release(pkt);
	s_sending = false;

	rtmp_client_publish_send(s_transport);
}

void rtmp_publish_aio_test(const char* host, const char* app, const char* stream, const char* file)
{
	socket_init();
	static char packet[2 * 1024 * 1024];
	snprintf(packet, sizeof(packet), "rtmp://%s/%s/%s", host, app, stream); // tcurl

	struct rtmp_client_transport_handler_t handler;
	handler.onsend = rtmp_client_publish_onsend;
	handler.onready = rtmp_client_publish_onready;

	s_transport = rtmp_client_transport_create(host, 1935, app, stream, packet, &handler, NULL);
	rtmp_client_transport_start(s_transport, 0);

	rtmp_client_push(file, s_transport);
	while(!s_packets.empty())
		system_sleep(1000); // wait for all data send to server

	rtmp_client_transport_destroy(s_transport);
	socket_cleanup();
}
