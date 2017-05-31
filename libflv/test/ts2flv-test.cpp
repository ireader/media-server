#include "flv-writer.h"
#include "flv-muxer.h"
#include "mpeg-ts.h"
#include "mpeg-ts-proto.h"
#include <stdio.h>
#include <assert.h>

static void on_flv_packet(void* flv, int type, const void* data, size_t bytes, uint32_t timestamp)
{
	flv_writer_input(flv, type, data, bytes, timestamp);
}

static void ts_packet(void* muxer, int avtype, int64_t pts, int64_t dts, void* data, size_t bytes)
{
	static int64_t s_pts = 0;
	if (0 == s_pts)
		s_pts = pts;
	pts -= s_pts;
	dts -= s_pts;

	if (PSI_STREAM_AAC == avtype)
	{
		flv_muxer_aac(muxer, data, bytes, (uint32_t)(pts/90), (uint32_t)(pts / 90));
	}
	else if (PSI_STREAM_MP3 == avtype)
	{
		flv_muxer_mp3(muxer, data, bytes, (uint32_t)(pts / 90), (uint32_t)(pts / 90));
	}
	else if (PSI_STREAM_H264 == avtype)
	{
		flv_muxer_avc(muxer, data, bytes, (uint32_t)(pts / 90), (uint32_t)(pts / 90));
	}
}

void ts2flv_test(const char* inputTS, const char* outputFLV)
{
	void* f = flv_writer_create(outputFLV);
	void* m = flv_muxer_create(on_flv_packet, f);

	unsigned char ptr[188];
	FILE* fp = fopen(inputTS, "rb");
	while (1 == fread(ptr, sizeof(ptr), 1, fp))
	{
		mpeg_ts_packet_dec(ptr, sizeof(ptr), ts_packet, m);
	}
	fclose(fp);

	flv_muxer_destroy(m);
	flv_writer_destroy(f);
}
