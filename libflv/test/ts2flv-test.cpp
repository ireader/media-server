#include "flv-writer.h"
#include "flv-muxer.h"
#include "mpeg4-aac.h"
#include "mpeg-ts.h"
#include "mpeg-ts-proto.h"
#include <stdio.h>
#include <assert.h>

static int on_flv_packet(void* flv, int type, const void* data, size_t bytes, uint32_t timestamp)
{
	return flv_writer_input(flv, type, data, bytes, timestamp);
}

static int on_ts_packet(void* param, int program, int /*stream*/, int avtype, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes)
{
	static int64_t s_pts = PTS_NO_VALUE;
	if (PTS_NO_VALUE == s_pts)
		s_pts = pts;
	pts -= s_pts;
	dts -= s_pts;

	flv_muxer_t* muxer = (flv_muxer_t*)param;
	if (PSI_STREAM_AAC == avtype)
	{
		int len = mpeg4_aac_adts_frame_length((const uint8_t*)data, bytes);
		while (len > 0 && len <= bytes)
		{
			flv_muxer_aac(muxer, data, len, (uint32_t)(pts / 90), (uint32_t)(pts / 90));
			data = (const uint8_t*)data + len;
			bytes -= len;
			len = mpeg4_aac_adts_frame_length((const uint8_t*)data, bytes);
		}
		assert(0 == bytes);
	}
	else if (PSI_STREAM_MP3 == avtype)
	{
		flv_muxer_mp3(muxer, data, bytes, (uint32_t)(pts / 90), (uint32_t)(dts / 90));
	}
	else if (PSI_STREAM_H264 == avtype)
	{
		flv_muxer_avc(muxer, data, bytes, (uint32_t)(pts / 90), (uint32_t)(dts / 90));
	}
	else if (PSI_STREAM_H265 == avtype)
	{
		flv_muxer_hevc(muxer, data, bytes, (uint32_t)(pts / 90), (uint32_t)(dts / 90));
	}
    
    return 0;
}

void ts2flv_test(const char* inputTS, const char* outputFLV)
{
	void* f = flv_writer_create(outputFLV);
	flv_muxer_t* m = flv_muxer_create(on_flv_packet, f);

	unsigned char ptr[188];
	FILE* fp = fopen(inputTS, "rb");
    ts_demuxer_t *ts = ts_demuxer_create(on_ts_packet, m);
    while (1 == fread(ptr, sizeof(ptr), 1, fp))
    {
        ts_demuxer_input(ts, ptr, sizeof(ptr));
    }
    ts_demuxer_flush(ts);
    ts_demuxer_destroy(ts);
	fclose(fp);

	flv_muxer_destroy(m);
	flv_writer_destroy(f);
}
