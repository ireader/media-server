#include "flv-writer.h"
#include "flv-muxer.h"
#include "mpeg4-aac.h"
#include "mpeg-ps.h"
#include "mpeg-types.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static int on_flv_packet(void* flv, int type, const void* data, size_t bytes, uint32_t timestamp)
{
	return flv_writer_input(flv, type, data, bytes, timestamp);
}

static int on_ps_packet(void* param, int stream, int codecid, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes)
{
	static int64_t s_pts = PTS_NO_VALUE;
	if (PTS_NO_VALUE == s_pts)
		s_pts = pts;
	pts -= s_pts;
	dts -= s_pts;

	flv_muxer_t* muxer = (flv_muxer_t*)param;
	if (PSI_STREAM_AAC == codecid)
	{
		int len = mpeg4_aac_adts_frame_length((const uint8_t*)data, bytes);
		while (len > 0 && len <= bytes)
		{
			flv_muxer_aac(muxer, data, len, (uint32_t)(pts / 90), (uint32_t)(pts / 90));

			mpeg4_aac_t aac;
			memset(&aac, 0, sizeof(aac));
			if (mpeg4_aac_adts_load((const uint8_t*)data, bytes, &aac) > 0 && aac.sampling_frequency > 0)
				pts += 1024 /*frame*/ * 1000 / aac.sampling_frequency;
			data = (const uint8_t*)data + len;
			bytes -= len;
			len = mpeg4_aac_adts_frame_length((const uint8_t*)data, bytes);
		}
		assert(0 == bytes);
	}
	else if (PSI_STREAM_MP3 == codecid)
	{
		flv_muxer_mp3(muxer, data, bytes, (uint32_t)(pts / 90), (uint32_t)(dts / 90));
	}
	else if (PSI_STREAM_H264 == codecid)
	{
		flv_muxer_avc(muxer, data, bytes, (uint32_t)(pts / 90), (uint32_t)(dts / 90));
	}
	else if (PSI_STREAM_H265 == codecid)
	{
		flv_muxer_hevc(muxer, data, bytes, (uint32_t)(pts / 90), (uint32_t)(dts / 90));
	}

	return 0;
}

void mpeg_ps_2_flv_test(const char* inputPS)
{
	char output[256] = { 0 };
	snprintf(output, sizeof(output) - 1, "%s.flv", inputPS);

	void* f = flv_writer_create(output);
	flv_muxer_t* m = flv_muxer_create(on_flv_packet, f);

	static uint8_t packet[2 * 1024 * 1024];
	FILE* fp = fopen(inputPS, "rb");

	size_t n, i = 0;
	ps_demuxer_t* ps = ps_demuxer_create(on_ps_packet, m);
	while ((n = fread(packet + i, 1, sizeof(packet) - i, fp)) > 0)
	{
		size_t r = ps_demuxer_input(ps, packet, n + i);
		memmove(packet, packet + r, n + i - r);
		i = n + i - r;
	}
	ps_demuxer_destroy(ps);
	fclose(fp);

	flv_muxer_destroy(m);
	flv_writer_destroy(f);
}
