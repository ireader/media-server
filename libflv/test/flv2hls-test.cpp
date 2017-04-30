#define _CRT_SECURE_NO_WARNINGS
#include "../libmpeg/include/mpeg-ps.h"
#include "../libhls/include/hls-vod.h"
#include "flv-reader.h"
#include "flv-demuxer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <assert.h>

inline const char* ftimestamp(uint32_t t, char* buf)
{
	sprintf(buf, "%u:%02u:%02u.%03u", t / 36000000, (t / 60000) % 60, (t / 1000) % 60, t % 1000);
	return buf;
}

static int hls_handler(void* /*param*/, const void* data, size_t bytes, int64_t /*pts*/, int64_t /*duration*/, uint64_t seq, char* name, size_t namelen)
{
	snprintf(name, namelen, "%I64d.ts", seq);

	FILE* fp = fopen(name, "wb");
	fwrite(data, 1, bytes, fp);
	fclose(fp);
	return 0;
}

inline char flv_type(int type)
{
	switch (type)
	{
	case FLV_AAC: return 'A';
	case FLV_MP3: return 'M';
	case FLV_AVC: return 'V';
	case FLV_AAC_HEADER: return 'a';
	case FLV_AVC_HEADER: return 'v';
	default: return '*';
	}
}
static void flv_handler(void* param, int type, const void* data, size_t bytes, uint32_t pts, uint32_t dts)
{
	static char s_pts[64], s_dts[64];
	static uint32_t v_pts = 0, v_dts = 0;
	static uint32_t a_pts = 0, a_dts = 0;

	printf("[%c] pts: %s, dts: %s, ", flv_type(type), ftimestamp(pts, s_pts), ftimestamp(dts, s_dts));

	switch (type)
	{
	case FLV_AAC:
	case FLV_MP3:
//		assert(0 == a_dts || dts >= a_dts);
		pts = (a_pts && pts < a_pts) ? a_pts : pts;
		dts = (a_dts && dts < a_dts) ? a_dts : dts;
		hls_vod_input(param, FLV_AAC==type?STREAM_AUDIO_AAC:STREAM_AUDIO_MP3, data, bytes, pts, dts, 0);

		printf("diff: %03d/%03d", (int)(pts - a_pts), (int)(dts - a_dts));
		a_pts = pts;
		a_dts = dts;
		break;

	case FLV_AVC:
		assert(0 == v_dts || dts >= v_dts);
		dts = (a_dts && dts < v_dts) ? v_dts : dts;
		hls_vod_input(param, STREAM_VIDEO_H264, data, bytes, pts, dts, 0);

		printf("diff: %03d/%03d", (int)(pts - v_pts), (int)(dts - v_dts));
		v_pts = pts;
		v_dts = dts;
		break;

	default:
		// nothing to do
		break;
	}

	printf("\n");
}

void flv2hls_test(const char* file)
{
	void* hls = hls_vod_create(10*1000, hls_handler, NULL);
	void* flv = flv_reader_create(file);
	void* demuxer = flv_demuxer_create(flv_handler, hls);

	int r, type;
	uint32_t timestamp;
	static char data[2 * 1024 * 1024];
	while ( (r = flv_reader_read(flv, &type, &timestamp, data, sizeof(data))) > 0)
	{
		flv_demuxer_input(demuxer, type, data, r, timestamp);
	}

	hls_vod_input(hls, STREAM_VIDEO_H264, NULL, 0, 0, 0, 1);

	flv_demuxer_destroy(demuxer);
	flv_reader_destroy(flv);

	size_t n = hls_vod_m3u8(hls, data, sizeof(data));
	FILE* fp = fopen("hls.m3u8", "wb");
	fwrite(data, 1, n, fp);
	fclose(fp);

	hls_vod_destroy(hls);
}
