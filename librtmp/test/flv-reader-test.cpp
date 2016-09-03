#include "rtmp-reader.h"
#include "flv-demuxer.h"
#include "flv-reader.h"
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
}

// flv_reader_test("53340.flv");
void flv_reader_test(const char* file)
{
	aac = fopen("audio.aac", "wb");
	h264 = fopen("video.h264", "wb");

	void* reader = flv_reader_create(file);
	void* flv = flv_demuxer_create(onFLV, NULL);

	int r = 0;
	while ((r = flv_reader_read(reader, packet, sizeof(packet))) > 0)
	{
		int n = flv_demuxer_input(flv, packet, r);
		if (n != r)
		{
			assert(0);
		}
	}

	flv_demuxer_destroy(flv);
	flv_reader_destroy(reader);

	fclose(aac);
	fclose(h264);
}
