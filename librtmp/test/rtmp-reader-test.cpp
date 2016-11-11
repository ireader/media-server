#include "rtmp-reader.h"
#include "flv-demuxer.h"
#include <assert.h>
#include <stdio.h>

static unsigned char packet[8 * 1024 * 1024];
static FILE* aac;
static FILE* h264;

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

static void rtmp_read(const char* url)
{
	void* rtmp = rtmp_reader_create(url);
	void* flv = flv_demuxer_create(onFLV, NULL);

	int r = 0;
	while ((r = rtmp_reader_read(rtmp, packet, sizeof(packet))) > 0)
	{
		int n = flv_demuxer_input(flv, packet, r);
		if (n != r)
		{
			assert(0);
		}
	}

	flv_demuxer_destroy(flv);
	rtmp_reader_destroy(rtmp);
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
