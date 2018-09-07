extern "C" {
#include "rtmp-internal.h"
#include "rtmp-msgtypeid.h"
}
#include "flv-reader.h"
#include "flv-writer.h"
#include "flv-proto.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static rtmp_t rtmp;
static int rtmp_client_send(void* /*param*/, const uint8_t* header, uint32_t len, const uint8_t* data, uint32_t bytes)
{
	if (len > 0)
	{
		assert(0 == rtmp_chunk_read(&rtmp, header, len));
	}
	if (bytes > 0)
	{
		assert(0 == rtmp_chunk_read(&rtmp, data, bytes));
	}
	return 0;
}

static int rtmp_client_onaudio(void* flv, const uint8_t* data, size_t bytes, uint32_t timestamp)
{
	return flv_writer_input(flv, FLV_TYPE_AUDIO, data, bytes, timestamp);
}

static int rtmp_client_onvideo(void* flv, const uint8_t* data, size_t bytes, uint32_t timestamp)
{
	return flv_writer_input(flv, FLV_TYPE_VIDEO, data, bytes, timestamp);
}

static int rtmp_client_onscript(void* flv, const uint8_t* data, size_t bytes, uint32_t timestamp)
{
	return flv_writer_input(flv, FLV_TYPE_SCRIPT, data, bytes, timestamp);
}

void rtmp_chunk_test(const char* flv)
{
	int r, type;
	uint32_t timestamp;
	static char packet[8 * 1024 * 1024];

	snprintf(packet, sizeof(packet), "%s.flv", flv);
	void* reader = flv_reader_create(flv);
	void* writer = flv_writer_create(packet);

	rtmp.parser.state = RTMP_PARSE_INIT;
	rtmp.in_chunk_size = 4096;
	rtmp.out_chunk_size = 4096;
	rtmp.param = writer;
	rtmp.send = rtmp_client_send;
	rtmp.onaudio = rtmp_client_onaudio;
	rtmp.onvideo = rtmp_client_onvideo;
	rtmp.onscript = rtmp_client_onscript;
	rtmp.out_packets[RTMP_CHANNEL_PROTOCOL].header.cid = RTMP_CHANNEL_PROTOCOL;
	rtmp.out_packets[RTMP_CHANNEL_INVOKE].header.cid = RTMP_CHANNEL_INVOKE;
	rtmp.out_packets[RTMP_CHANNEL_AUDIO].header.cid = RTMP_CHANNEL_AUDIO;
	rtmp.out_packets[RTMP_CHANNEL_VIDEO].header.cid = RTMP_CHANNEL_VIDEO;
	rtmp.out_packets[RTMP_CHANNEL_DATA].header.cid = RTMP_CHANNEL_DATA;
	
	while ((r = flv_reader_read(reader, &type, &timestamp, packet, sizeof(packet))) > 0)
	{
		if (FLV_TYPE_AUDIO == type)
		{
			struct rtmp_chunk_header_t header;
			header.fmt = RTMP_CHUNK_TYPE_1; // enable compact header
			header.cid = RTMP_CHANNEL_AUDIO;
			header.timestamp = timestamp;
			header.length = (uint32_t)r;
			header.type = RTMP_TYPE_AUDIO;
			header.stream_id = 1;
			assert(0 == rtmp_chunk_write(&rtmp, &header, (const uint8_t*)packet));
		}
		else if (FLV_TYPE_VIDEO == type)
		{
			struct rtmp_chunk_header_t header;
			header.fmt = RTMP_CHUNK_TYPE_1; // enable compact header
			header.cid = RTMP_CHANNEL_VIDEO;
			header.timestamp = timestamp;
			header.length = (uint32_t)r;
			header.type = RTMP_TYPE_VIDEO;
			header.stream_id = 1;
			assert(0 == rtmp_chunk_write(&rtmp, &header, (const uint8_t*)packet));
		}
		else if (FLV_TYPE_SCRIPT == type)
		{
			struct rtmp_chunk_header_t header;
			header.fmt = RTMP_CHUNK_TYPE_1; // enable compact header
			header.cid = RTMP_CHANNEL_INVOKE;
			header.timestamp = timestamp;
			header.length = (uint32_t)r;
			header.type = RTMP_TYPE_DATA;
			header.stream_id = 1;
			assert(0 == rtmp_chunk_write(&rtmp, &header, (const uint8_t*)packet));
		}
		else
		{
			assert(0);
		}
	}

	flv_reader_destroy(reader);
	flv_writer_destroy(writer);
}
