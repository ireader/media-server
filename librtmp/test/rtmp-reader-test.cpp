#include "sys/sock.h"
#include "rtmp-client.h"
#include "flv-demuxer.h"
#include "flv-writer.h"
#include <assert.h>
#include <stdio.h>

static unsigned char packet[8 * 1024 * 1024];
static FILE* aac;
static FILE* h264;
static FILE* flv;

static void be_write_uint32(uint8_t* ptr, uint32_t val)
{
	ptr[0] = (uint8_t)((val >> 24) & 0xFF);
	ptr[1] = (uint8_t)((val >> 16) & 0xFF);
	ptr[2] = (uint8_t)((val >> 8) & 0xFF);
	ptr[3] = (uint8_t)(val & 0xFF);
}

static void onFLV(void* param, int type, const void* data, size_t bytes, uint32_t pts, uint32_t dts)
{
	if (FLV_AAC == type)
	{
		fwrite(data, bytes, 1, aac);
	}
	else if (FLV_AVC == type)
	{
		fwrite(data, bytes, 1, h264);
	}
	else
	{
		assert(0);
	}
}

static int rtmp_send(void* param, const void* data, size_t bytes)
{
	socket_t* socket = (socket_t*)param;
	return socket_send(*socket, data, bytes, 0);
}

static void rtmp_onerror(void* param, int code, const char* msg)
{
	printf("rtmp_onerror code: %d, msg: %s\n", code, msg);
}

static inline void flv_write_tag(uint8_t* tag, uint8_t type, uint32_t bytes, uint32_t timestamp)
{
	// TagType
	tag[0] = type & 0x1F;

	// DataSize
	tag[1] = (bytes >> 16) & 0xFF;
	tag[2] = (bytes >> 8) & 0xFF;
	tag[3] = bytes & 0xFF;

	// Timestamp
	tag[4] = (timestamp >> 16) & 0xFF;
	tag[5] = (timestamp >> 8) & 0xFF;
	tag[6] = (timestamp >> 0) & 0xFF;
	tag[7] = (timestamp >> 24) & 0xFF; // Timestamp Extended

	// StreamID(Always 0)
	tag[8] = 0;
	tag[9] = 0;
	tag[10] = 0;
}

static void rtmp_onaudio(void* param, const char* data, size_t bytes, uint32_t timestamp)
{
	uint8_t tag[11];
	uint8_t be[4];
	flv_write_tag(tag, 8, bytes, timestamp);
	fwrite(tag, 11, 1, flv);
	fwrite(data, bytes, 1, flv);
	
	be_write_uint32(be, bytes + 11);
	fwrite(be, 4, 1, flv); // TAG size
}

static void rtmp_onvideo(void* param, const void* data, size_t bytes, uint32_t timestamp)
{
	uint8_t tag[11];
	uint8_t be[4];
	flv_write_tag(tag, 9, bytes, timestamp);
	fwrite(tag, 11, 1, flv);
	fwrite(data, bytes, 1, flv);

	be_write_uint32(be, bytes + 11);
	fwrite(be, 4, 1, flv); // TAG size
}

static void rtmp_onmeta(void* param, const char* data, size_t bytes)
{
}

static void rtmp_read(const char* url)
{
	socket_init();
	socket_t socket = socket_connect_host("192.168.31.129", 1935, 2000);
	socket_setnonblock(socket, 0);

	struct rtmp_client_handler_t handler;
	handler.send = rtmp_send;
	handler.onerror = rtmp_onerror;
	handler.onmeta = rtmp_onmeta;
	handler.onaudio = rtmp_onaudio;
	handler.onvideo = rtmp_onvideo;
	void* rtmp = rtmp_client_create("vod", "1.flv", "rtmp://192.168.31.129:1935/vod", &socket, &handler);

	flv = fopen("1.flv", "wb");
	uint8_t header[9 + 4];
	memcpy(header, "FLV", 3); // FLV signature
	header[3] = 0x01; // File version
	header[4] = 0x06; // Type flags (audio & video)
	be_write_uint32(header + 5, 9); // Data offset
	be_write_uint32(header + 9, 0); // PreviousTagSize0(Always 0)
	fwrite(header, 1, sizeof(header), flv);

	int r = rtmp_client_start(rtmp, 1);
	while ((r = socket_recv(socket, packet, sizeof(packet), 0)) > 0)
	{
		r = rtmp_client_input(rtmp, packet, r);
	}

	rtmp_client_stop(rtmp);
	fclose(flv);
	rtmp_client_destroy(&rtmp);
	socket_close(socket);
	socket_cleanup();
}

// rtmp_reader_test("rtmp://strtmpplay.cdn.suicam.com/carousel/51632");
void rtmp_reader_test(const char* url)
{
	aac = fopen("audio.aac", "wb");
	h264 = fopen("video.h264", "wb");

	//socket_init();
	rtmp_read(url);
	//socket_cleanup();

	fclose(aac);
	fclose(h264);
}
