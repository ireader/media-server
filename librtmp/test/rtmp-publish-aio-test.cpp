#include "sys/sock.h"
#include "sys/system.h"
#include "sys/sync.hpp"
#include "rtmp-client-transport.h"
#include "flv-reader.h"
#include "flv-proto.h"
#include "time64.h"
#include <stdio.h>
#include <assert.h>
#include <list>

struct TFLVPacket
{
	uint8_t* data;
	size_t bytes;
	uint32_t timestamp;
	int avtype;
};

static ThreadLocker s_locker;
static std::list<struct TFLVPacket*> s_packets;
static bool s_sending = true;
static void* s_transport;

static void rtmp_client_publish_send(void* transport)
{
	if (!s_sending && !s_packets.empty())
	{
		s_sending = true;
		TFLVPacket* pkt = s_packets.front();
		int r = rtmp_client_transport_sendpacket(transport, pkt->avtype, pkt->data, pkt->bytes, pkt->timestamp);
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

		TFLVPacket* pkt = (TFLVPacket*)malloc(sizeof(TFLVPacket) + r);
		pkt->data = (uint8_t*)(pkt + 1);
		memcpy(pkt->data, packet, r);
		pkt->bytes = r;
		pkt->avtype = FLV_TYPE_AUDIO == type ? 0 : 1;
		pkt->timestamp = timestamp;

		AutoThreadLocker locker(s_locker);
		s_packets.push_back(pkt);
		rtmp_client_publish_send(transport);
	}

	flv_reader_destroy(f);
}

static void rtmp_client_publish_onready(void*)
{
	AutoThreadLocker locker(s_locker);
	s_sending = false;
	rtmp_client_publish_send(s_transport);
}

static void rtmp_client_publish_onsend(void*)
{
	AutoThreadLocker locker(s_locker);
	TFLVPacket* pkt = s_packets.front();
	s_packets.pop_front();
	free(pkt);
	s_sending = false;

	rtmp_client_publish_send(s_transport);
}

void rtmp_publish_aio_test(const char* host, const char* app, const char* stream, const char* file)
{
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
}
